add_mlir_dialect_library(MLIRTTIRAnalysis
  AllocationPlanner.cpp
  AssociatedDMAWaits.cpp

  ADDITIONAL_HEADER_DIRS
  ${PROJECT_SOURCE_DIR}/include/ttmlir

  DEPENDS
  MLIRTTIROpsIncGen
  MLIRTTOpsIncGen

  LINK_LIBS PUBLIC
  MLIRAnalysis
  )
