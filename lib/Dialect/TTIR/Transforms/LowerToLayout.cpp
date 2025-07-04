// SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#include "ttmlir/Dialect/TTCore/IR/TTCore.h"
#include "ttmlir/Dialect/TTIR/IR/TTIROps.h"
#include "ttmlir/Dialect/TTIR/Transforms/Passes.h"
#include "ttmlir/Dialect/TTIR/Utils/UniformTypeRewriter.h"
#include "ttmlir/Utils.h"

#include "mlir/Dialect/Bufferization/IR/Bufferization.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

namespace mlir::tt::ttir {
#define GEN_PASS_DEF_TTIRLOWERTOLAYOUT
#include "ttmlir/Dialect/TTIR/Transforms/Passes.h.inc"

class TTIRLowerToLayoutRewriter : public OpRewritePattern<ToLayoutOp> {
public:
  using OpRewritePattern<ToLayoutOp>::OpRewritePattern;

  static LogicalResult lowerLayoutChange(PatternRewriter &rewriter,
                                         ToLayoutOp op) {
    assert(false &&
           "TODO issue https://github.com/tenstorrent/tt-mlir/issues/3037");
  }

  static LogicalResult lowerSystemLayoutChange(PatternRewriter &rewriter,
                                               ToLayoutOp op) {
    MetalLayoutAttr inputLayout = op.getOrCreateInputLayout();
    MetalLayoutAttr outputLayout = op.getOrCreateOutputLayout();
    bool inputSystem = inputLayout.getMemorySpace() == MemorySpace::System;
    bool outputSystem = outputLayout.getMemorySpace() == MemorySpace::System;
    assert(inputSystem != outputSystem &&
           "one of input or output must be system for now");
    if (op.getLayout()) {
      // Already lowered.
      return failure();
    }

    rewriter.replaceOpWithNewOp<ToLayoutOp>(op, op.getInput(), op.getOutput(),
                                            inputSystem ? outputLayout
                                                        : inputLayout);
    return success();
  }

  static LogicalResult lowerDatamovementGeneric(PatternRewriter &rewriter,
                                                ToLayoutOp op) {
    MetalLayoutAttr inputLayout = op.getOrCreateInputLayout();
    MetalLayoutAttr outputLayout = op.getOrCreateOutputLayout();
    if (inputLayout.getMemorySpace() == MemorySpace::System ||
        outputLayout.getMemorySpace() == MemorySpace::System) {
      // To/From host mem is a special case that is lowered to
      // ttmetal.enqueue_write_buffer or ttmetal.enqueue_read_buffer
      return lowerSystemLayoutChange(rewriter, op);
    }

    auto view = rewriter
                    .create<ViewLayoutOp>(op.getLoc(), op.getOutput().getType(),
                                          op.getInput())
                    .getResult();

    assert(inputLayout.getLogicalShape().size() ==
           outputLayout.getLogicalShape().size());

    const size_t logicalRank = inputLayout.getLogicalShape().size();

    ArrayAttr indexingMaps, iteratorTypes;
    std::tie(indexingMaps, iteratorTypes) =
        GenericOp::buildParallelAffineMapsAndIteratorTypes(
            rewriter, /*arity=*/2, logicalRank);
    rewriter.replaceOpWithNewOp<GenericOp>(
        op, view, op.getOutput(),
        [&](OpBuilder &builder, Location loc, ValueRange blockArgs) {
          auto dma = builder.create<ttir::DMAOp>(
              loc, view, mlir::cast<AffineMapAttr>(indexingMaps[0]),
              blockArgs[1]);
          builder.create<ttir::DMAWaitOp>(loc, dma);
        },
        ThreadType::Datamovement);

    return success();
  }

  static LogicalResult lowerFormatConversionGeneric(PatternRewriter &rewriter,
                                                    ToLayoutOp op) {
    auto inputType = mlir::cast<RankedTensorType>(op.getInput().getType());
    auto outputType = mlir::cast<RankedTensorType>(op.getOutput().getType());
    bool inputTiled = tt::isTiled(inputType);
    bool outputTiled = tt::isTiled(outputType);
    assert(inputTiled != outputTiled &&
           "one of input or output must be tiled for now");

    rewriter.replaceOpWithNewOp<GenericOp>(
        op, op.getInput(), op.getOutput(),
        [=](OpBuilder &builder, Location loc, ValueRange blockArgs) {
          if (inputTiled) {
            builder.create<TileUntilizeBlockOp>(loc, blockArgs[0],
                                                blockArgs[1]);
          } else {
            builder.create<TileTilizeBlockOp>(loc, blockArgs[0], blockArgs[1]);
          }
        });

    return success();
  }

  LogicalResult matchAndRewrite(ToLayoutOp op,
                                PatternRewriter &rewriter) const final {
    auto components = op.compoundComponents();

    if (components.isCompound()) {
      return failure();
    }

    if (components.isLayoutChange) {
      return lowerLayoutChange(rewriter, op);
    }

    if (components.isGridChange || components.isMemorySpaceChange) {
      return lowerDatamovementGeneric(rewriter, op);
    }

    if (components.isFormatChange) {
      return lowerFormatConversionGeneric(rewriter, op);
    }

    llvm_unreachable("Unknown compound component");
  }
};

class TTIRSplitCompoundLayoutRewriter : public OpRewritePattern<ToLayoutOp> {
public:
  TTIRSplitCompoundLayoutRewriter(MLIRContext *context)
      : OpRewritePattern(context, PatternBenefit(2)) {}

  ttir::ToLayoutOp createToLayoutOp(PatternRewriter &rewriter, Location loc,
                                    Value input,
                                    RankedTensorType desiredType) const {
    // Create empty tensor with desired type and layout
    auto layout = mlir::cast<MetalLayoutAttr>(desiredType.getEncoding());
    auto output = rewriter.create<ttir::EmptyOp>(
        loc, desiredType.getShape(), desiredType.getElementType(), layout);
    return rewriter.create<ttir::ToLayoutOp>(loc, input, output);
  }

  Value bounce(PatternRewriter &rewriter, ToLayoutOp op,
               RankedTensorType bounceType) const {
    auto bounced =
        createToLayoutOp(rewriter, op.getLoc(), op.getInput(), bounceType);
    return rewriter
        .replaceOpWithNewOp<ttir::ToLayoutOp>(op, bounced->getResult(0),
                                              op.getOutput())
        ->getResult(0);
  }

  // Helper to create a new tensor type with modified layout
  RankedTensorType
  createModifiedType(MLIRContext *ctx, RankedTensorType baseType,
                     MetalLayoutAttr baseLayout,
                     std::optional<MemorySpace> newMemSpace = {},
                     std::optional<ArrayRef<int64_t>> newGrid = {},
                     std::optional<Type> newElementType = {},
                     std::optional<ArrayRef<int64_t>> newTileShape = {}) const {
    // Use existing values if not overridden
    auto memSpace = newMemSpace.value_or(baseLayout.getMemorySpace());
    auto maybeBaseLayout =
        mlir::dyn_cast_or_null<MetalLayoutAttr>(baseType.getEncoding());
    const bool baseTypeHasLayout = maybeBaseLayout != nullptr;

    // We need to create an owning version of gridShape for the case where we
    // default 1-fill it, which makes this more complex/ugly.
    SmallVector<int64_t, 2> gridShape;
    if (newGrid.has_value()) {
      gridShape.assign(newGrid->begin(), newGrid->end());
    } else if (baseTypeHasLayout) {
      auto tempGrid = maybeBaseLayout.getGridShape(baseType);
      gridShape.assign(tempGrid.begin(), tempGrid.end());
    }

    auto elementType = newElementType.value_or(baseType.getElementType());
    auto tileShape = newTileShape.has_value()
                         ? *newTileShape
                         : getTensorTileShapeOrEmpty(baseType);

    // Create new layout
    auto newLayout =
        baseTypeHasLayout
            ? MetalLayoutAttr::get(ctx, baseLayout.getLogicalShape(),
                                   gridShape.size(), baseLayout.getOobVal(),
                                   memSpace,
                                   maybeBaseLayout.getCollapseIntervals(),
                                   maybeBaseLayout.getDimAlignments())
            : MetalLayoutAttr::get(ctx, baseLayout.getLogicalShape(),
                                   gridShape.size(), baseLayout.getOobVal(),
                                   memSpace);

    // For physical shape derivation, use tile shape ONLY if element type is
    // tiled
    ArrayRef<int64_t> tileShapeForPhysical;
    if (mlir::isa<TileType>(elementType)) {
      // Element type is tiled: need tile shape to compute tile counts
      tileShapeForPhysical = tileShape;
    } else {
      // Element type is not tiled: empty to compute element counts
      tileShapeForPhysical = {};
    }

    // Derive physical shape
    auto physicalShape = MetalLayoutAttr::derivePhysicalShape(
        baseLayout.getLogicalShape(), gridShape, tileShapeForPhysical,
        newLayout.getCollapseIntervals(), newLayout.getDimAlignments());

    return RankedTensorType::get(physicalShape, elementType, newLayout);
  }

  LogicalResult matchAndRewrite(ToLayoutOp op,
                                PatternRewriter &rewriter) const final {
    auto components = op.compoundComponents();

    if (!components.isCompound()) {
      return failure();
    }

    auto inputType = mlir::cast<RankedTensorType>(op.getInput().getType());
    auto outputType = mlir::cast<RankedTensorType>(op.getOutput().getType());
    const bool hasInputLayout = inputType.getEncoding() != nullptr;
    const bool hasOutputLayout = outputType.getEncoding() != nullptr;
    auto inputLayout = op.getOrCreateInputLayout();
    auto outputLayout = op.getOrCreateOutputLayout();

    bool inputL1 = inputLayout.getMemorySpace() == MemorySpace::DeviceL1;
    bool outputL1 = outputLayout.getMemorySpace() == MemorySpace::DeviceL1;

    // First prioritize moving the data into L1 so we can work with it in L1
    if (!inputL1) {
      // Read into L1, then do other conversions.
      // If we're going from no grid -> grid, we need to use the output grid
      // size (1s filled).
      if (!hasInputLayout && hasOutputLayout) {
        auto gridShape = llvm::SmallVector<int64_t>(
            outputLayout.getGridShape(outputType).size(), 1);
        auto bounceType =
            createModifiedType(rewriter.getContext(), inputType, inputLayout,
                               MemorySpace::DeviceL1, gridShape);
        bounce(rewriter, op, bounceType);
      } else {
        // For other cases, we want to use input's current grid
        auto bounceType =
            createModifiedType(rewriter.getContext(), inputType, inputLayout,
                               MemorySpace::DeviceL1);
        bounce(rewriter, op, bounceType);
      }
    } else if (!outputL1) {
      // Convert to L1 first, then do other conversions.
      assert(inputL1 && "input should guaranteed be in L1 because of the "
                        "previous case");
      // Conversely, if we're going from grid -> no grid, we need to use the
      // input grid size (1s filled).
      if (!hasOutputLayout && hasInputLayout) {
        auto gridShape = llvm::SmallVector<int64_t>(
            inputLayout.getGridShape(inputType).size(), 1);
        auto bounceType =
            createModifiedType(rewriter.getContext(), outputType, outputLayout,
                               MemorySpace::DeviceL1, gridShape);
        bounce(rewriter, op, bounceType);
      } else {
        // For other cases, we want to use input's current grid
        auto bounceType =
            createModifiedType(rewriter.getContext(), outputType, outputLayout,
                               MemorySpace::DeviceL1);
        bounce(rewriter, op, bounceType);
      }
    } else if (tt::isTiled(inputType) != tt::isTiled(outputType)) {
      // Prioritize moving tiled data
      if (tt::isTiled(inputType)) {
        auto bounceType =
            createModifiedType(rewriter.getContext(), outputType, outputLayout,
                               /*memSpace=*/{},
                               /*grid=*/{}, inputType.getElementType(),
                               getTensorTileShapeOrEmpty(inputType));
        bounce(rewriter, op, bounceType);
      } else {
        assert(tt::isTiled(outputType));
        auto bounceType =
            createModifiedType(rewriter.getContext(), inputType, inputLayout,
                               /*memSpace=*/{},
                               /*grid=*/{}, outputType.getElementType(),
                               tt::getTensorTileShape(outputType));
        bounce(rewriter, op, bounceType);
      }
    } else if (components.isLayoutChange && tt::isTiled(inputType)) {
      // For now to flexibly support layout changes, we need to bounce to scalar
      // first Get scalar element type
      Type scalarType = inputType.getElementType();
      if (auto tileType = mlir::dyn_cast<TileType>(scalarType)) {
        scalarType = tileType.getElementType();
      }

      // Create untiled version with scalar type
      auto bounceType =
          createModifiedType(rewriter.getContext(), inputType, inputLayout,
                             /*memSpace=*/{}, /*grid=*/{}, scalarType,
                             /*tileShape=*/std::nullopt);
      bounce(rewriter, op, bounceType);
    } else if (components.isGridChange) {
      assert(!components.isLayoutChange &&
             "Changing layout and grid at the same time is currently "
             "not supported");
      // Keep output layout but with input's grid
      auto bounceType = createModifiedType(
          rewriter.getContext(), outputType, outputLayout,
          /*memSpace=*/{}, inputLayout.getGridShape(inputType));
      bounce(rewriter, op, bounceType);
    } else {
      // Note we should eventually support DRAM <-> DRAM, or System <-> System
      // w/ format conversion via streaming supported
      assert(false && "Unsupported compound layout change");
      return failure();
    }

    return success();
  }
};

class TTIRLowerToLayout
    : public impl::TTIRLowerToLayoutBase<TTIRLowerToLayout> {
public:
  using impl::TTIRLowerToLayoutBase<TTIRLowerToLayout>::TTIRLowerToLayoutBase;

  void runOnOperation() final {
    RewritePatternSet patterns(&getContext());
    patterns.add<TTIRSplitCompoundLayoutRewriter, TTIRLowerToLayoutRewriter>(
        &getContext());
    if (failed(applyPatternsGreedily(getOperation(), std::move(patterns)))) {
      signalPassFailure();
      return;
    }
  }
};

} // namespace mlir::tt::ttir
