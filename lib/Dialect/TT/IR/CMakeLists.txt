add_mlir_dialect_library(MLIRTTDialect
        TTOpsTypes.cpp
        TTDialect.cpp
        TTOps.cpp
        Utils.cpp

        ADDITIONAL_HEADER_DIRS
        ${PROJECT_SOURCE_DIR}/include/ttmlir

        DEPENDS
        MLIRTTOpsIncGen

        LINK_LIBS PUBLIC
        MLIRQuantDialect
        )
