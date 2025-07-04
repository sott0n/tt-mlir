# SPDX-FileCopyrightText: (c) 2024 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

# RUN: %python %s | FileCheck %s

from ttmlir.ir import *
from ttmlir.dialects import ttcore

ctx = Context()


def updiv(n, d):
    return (n + d - 1) // d


def volume(shape):
    vol = 1
    for dim in shape:
        vol *= dim
    return vol


def inferWorkerGridMap(grid, physicalGrid=[8, 8], mesh=[1]):
    assert len(grid) >= 2
    mesh = mesh.copy()
    while len(mesh) < len(grid):
        mesh.append(1)
    totalDevices = volume(mesh)
    p0 = AffineConstantExpr.get(physicalGrid[0], ctx)
    p1 = AffineConstantExpr.get(physicalGrid[1], ctx)
    dZ = AffineConstantExpr.get(0, ctx)
    dY = AffineDimExpr.get(len(grid) - 2, ctx)
    dX = AffineDimExpr.get(len(grid) - 1, ctx)
    if totalDevices > 1:
        v = mesh[-1]
        if mesh[-1] > 1:
            dZ = dZ + AffineFloorDivExpr.get(dX, p1)
        if mesh[-2] > 1:
            dZ = dZ + AffineFloorDivExpr.get(dY, p0) * v
        for d in range(len(mesh) - 3, -1, -1):
            v *= mesh[d + 1]
            dn = AffineDimExpr.get(d, ctx)
            dZ = dn * v + dZ
    exprs = [
        dZ,
        dY % p0 if mesh[-2] > 1 else dY,
        dX % p1 if mesh[-1] > 1 else dX,
    ]
    return AffineMap.get(len(grid), 0, exprs, ctx)


def inferMemoryMap(grid):
    assert len(grid) <= 4
    zero = AffineConstantExpr.get(0, ctx)
    exprs = [AffineDimExpr.get(i, ctx) for i in range(len(grid))]
    while len(exprs) < 4:
        exprs.insert(0, zero)
    return AffineMap.get(len(grid), len(grid), exprs, ctx)


def createDeviceAttr(
    grid,
    physicalGrid=[8, 8],
    deviceStartIdx=0,
    workerGridMap=None,
    system_desc=None,
    mesh_shape=[1],
):
    if system_desc is not None:
        return ttcore.ir.DeviceAttr.from_system_desc(ctx, system_desc, mesh_shape)
    workerGridMap = (
        workerGridMap
        if workerGridMap is not None
        else inferWorkerGridMap(grid, physicalGrid, mesh=mesh_shape)
    )
    l1Map = inferMemoryMap(grid)
    dramMap = inferMemoryMap(grid)
    totalDevices = volume(mesh_shape)
    return ttcore.ir.DeviceAttr.get(
        ctx,
        grid,
        workerGridMap,
        l1Map,
        dramMap,
        mesh_shape,
        list(range(deviceStartIdx, deviceStartIdx + totalDevices)),
    )


c = lambda v: AffineConstantExpr.get(v, ctx)
d = lambda v: AffineDimExpr.get(v, ctx)
amap = lambda ndims, results: AffineMap.get(ndims, 0, results, ctx)
floordiv = AffineFloorDivExpr.get

c0 = c(0)
d0 = d(0)
d1 = d(1)
d2 = d(2)

print("=== From SystemDesc ===")
# CHECK: ttcore.device<workerGrid = #ttcore.grid<8x8, (d0, d1) -> (0, d0, d1)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 1, chipIds = [0]>
print(
    "", createDeviceAttr([8, 8], system_desc=ttcore.ir.SystemDescAttr.get_default(ctx))
)

# ------------------------------------------------------------------------------

print("=== Simple single device ===")
# CHECK: ttcore.device<workerGrid = #ttcore.grid<8x8, (d0, d1) -> (0, d0, d1)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 1, chipIds = [0]>
print("", createDeviceAttr([8, 8]))

# ------------------------------------------------------------------------------

print("\n=== Data parallel over batch ===")
# CHECK: ttcore.device<workerGrid = #ttcore.grid<2x8x8, (d0, d1, d2) -> (d0, d1, d2)>, l1Map = [[M:.*]], dramMap = [[M:.*]], meshShape = 2x1x1, chipIds = [0, 1]>
print("divide batch by 2\n", createDeviceAttr([2, 8, 8], mesh_shape=[2, 1, 1]))
# CHECK: ttcore.device<workerGrid = #ttcore.grid<4x8x8, (d0, d1, d2) -> (d0, d1, d2)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 4x1x1, chipIds = [0, 1, 2, 3]>
print("divide batch by 4\n", createDeviceAttr([4, 8, 8], mesh_shape=[4, 1, 1]))

# ------------------------------------------------------------------------------

print("\n=== Data parallel over 2d ===")
# CHECK: ttcore.device<workerGrid = #ttcore.grid<8x16, (d0, d1) -> (d1 floordiv 8, d0, d1 mod 8)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 1x2, chipIds = [0, 1]>
print(
    "Reinterpret 2 devices as grid side by side, 1x2 mesh\n",
    createDeviceAttr([8, 16], mesh_shape=[1, 2]),
)
# CHECK: ttcore.device<workerGrid = #ttcore.grid<16x8, (d0, d1) -> (d0 floordiv 8, d0 mod 8, d1)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 2x1, chipIds = [0, 1]>
print(
    "Reinterpret 2 devices as grid top to bottom, 2x1 mesh\n",
    createDeviceAttr([16, 8], mesh_shape=[2, 1]),
)
# CHECK: ttcore.device<workerGrid = #ttcore.grid<16x32, (d0, d1) -> (d1 floordiv 8 + (d0 floordiv 8) * 4, d0 mod 8, d1 mod 8)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 2x4, chipIds = [0, 1, 2, 3, 4, 5, 6, 7]>
print("8 devices 2x4 mesh\n", createDeviceAttr([16, 32], mesh_shape=[2, 4]))
# CHECK: ttcore.device<workerGrid = #ttcore.grid<32x16, (d0, d1) -> (d1 floordiv 8 + (d0 floordiv 8) * 2, d0 mod 8, d1 mod 8)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 4x2, chipIds = [0, 1, 2, 3, 4, 5, 6, 7]>
print("8 devices 4x2 mesh\n", createDeviceAttr([32, 16], mesh_shape=[4, 2]))

# ------------------------------------------------------------------------------

print("\n=== Data parallel over 2d and batch (3d) ===")
# CHECK: ttcore.device<workerGrid = #ttcore.grid<2x8x16, (d0, d1, d2) -> (d0 * 2 + d2 floordiv 8, d1, d2 mod 8)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 2x1x2, chipIds = [0, 1, 2, 3]>
print(
    "divide batch by 2, 2x1x2 mesh\n",
    createDeviceAttr([2, 8, 16], mesh_shape=[2, 1, 2]),
)
# CHECK: ttcore.device<workerGrid = #ttcore.grid<3x24x8, (d0, d1, d2) -> (d0 * 3 + d1 floordiv 8, d1 mod 8, d2)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 3x3x1, chipIds = [0, 1, 2, 3, 4, 5, 6, 7, 8]>
print(
    "divide batch by 3, 3x3x1 mesh\n",
    createDeviceAttr([3, 24, 8], mesh_shape=[3, 3, 1]),
)

# ------------------------------------------------------------------------------

print("\n=== nD ===")
# CHECK: ttcore.device<workerGrid = #ttcore.grid<3x2x8x8, (d0, d1, d2, d3) -> (d0 * 2 + d1, d2, d3)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 3x2x1x1, chipIds = [0, 1, 2, 3, 4, 5]>
print("", createDeviceAttr([3, 2, 8, 8], mesh_shape=[3, 2, 1, 1]))

# ------------------------------------------------------------------------------

print("\n=== Data parallel batch on single device ===")
# CHECK: ttcore.device<workerGrid = #ttcore.grid<2x4x8, (d0, d1, d2) -> (0, d0 * 4 + d1, d2)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 1, chipIds = [0]>
print(
    "divide batch by 2, top 4 rows get batch 0, bottom 4 rows get batch 1\n",
    createDeviceAttr([2, 4, 8], workerGridMap=amap(3, [c0, d0 * 4 + d1, d2])),
)

# ------------------------------------------------------------------------------

print("\n=== Pipeline parallel ===")
# CHECK: ttcore.device<workerGrid = #ttcore.grid<2x8x16, (d0, d1, d2) -> (d0 * 2 + d2 floordiv 8, d1, d2 mod 8)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 2x1x2, chipIds = [0, 1, 2, 3]>
print(
    "view devices 0-3 in one way\n",
    createDeviceAttr([2, 8, 16], deviceStartIdx=0, mesh_shape=[2, 1, 2]),
)
# CHECK: ttcore.device<workerGrid = #ttcore.grid<16x16, (d0, d1) -> (d1 floordiv 8 + (d0 floordiv 8) * 2, d0 mod 8, d1 mod 8)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 2x2, chipIds = [4, 5, 6, 7]>
print(
    "view devices 4-7 in another way\n",
    createDeviceAttr([16, 16], deviceStartIdx=4, mesh_shape=[2, 2]),
)

# ------------------------------------------------------------------------------

print("\n=== Reinterpreted Grids ===")
# CHECK: ttcore.device<workerGrid = #ttcore.grid<8x8, (d0, d1) -> (0, d1, d0)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 1, chipIds = [0]>
print("transposed\n", createDeviceAttr([8, 8], workerGridMap=amap(2, [c0, d1, d0])))
# CHECK: ttcore.device<workerGrid = #ttcore.grid<1x64, (d0, d1) -> (0, d0 * 8 + d1 floordiv 8, d1 mod 8)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 1, chipIds = [0]>
print(
    "extra wide\n",
    createDeviceAttr(
        [1, 64], workerGridMap=amap(2, [c0, d0 * 8 + floordiv(d1, c(8)), d1 % 8])
    ),
)
# CHECK: ttcore.device<workerGrid = #ttcore.grid<64x1, (d0, d1) -> (0, d1 * 8 + d0 floordiv 8, d0 mod 8)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 1, chipIds = [0]>
print(
    "extra tall transposed\n",
    createDeviceAttr(
        [64, 1],
        workerGridMap=amap(2, [c0, d1 * 8 + floordiv(d0, c(8)), d0 % 8]),
    ),
)
# CHECK: ttcore.device<workerGrid = #ttcore.grid<8x8, (d0, d1) -> (0, d0, (d0 + d1) mod 8)>, l1Map = [[L1:.*]], dramMap = [[DRAM:.*]], meshShape = 1, chipIds = [0]>
print(
    "staircase systolic\n",
    createDeviceAttr([8, 8], workerGridMap=amap(2, [c0, d0, (d0 + d1) % 8])),
)
