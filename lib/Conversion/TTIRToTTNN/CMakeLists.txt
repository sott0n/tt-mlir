add_mlir_conversion_library(TTMLIRTTIRToTTNN
  TTIRToTTNN.cpp
  TTIRToTTNNPass.cpp
  Utils.cpp

  ADDITIONAL_HEADER_DIRS
  ${PROJECT_SOURCE_DIR}/include/ttmlir/Conversion/TTIRToTTNN

  DEPENDS
  TTMLIRConversionPassIncGen

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRPass
  TTMLIRTTNNUtils
)
