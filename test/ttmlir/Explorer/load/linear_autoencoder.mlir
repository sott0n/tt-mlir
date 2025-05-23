// RUN: ttmlir-opt --ttir-to-ttnn-backend-pipeline %s

module @LinearAE attributes {} {
  func.func @forward(%arg0: tensor<1x784xf32> {ttir.name = "input_1"}, %arg1: tensor<784x128xf32> {ttir.name = "encoder_lin1.weight"}, %arg2: tensor<128xf32> {ttir.name = "encoder_lin1.bias"}, %arg3: tensor<128x64xf32> {ttir.name = "encoder_lin2.weight"}, %arg4: tensor<64xf32> {ttir.name = "encoder_lin2.bias"}, %arg5: tensor<64x12xf32> {ttir.name = "encoder_lin3.weight"}, %arg6: tensor<12xf32> {ttir.name = "encoder_lin3.bias"}, %arg7: tensor<12x3xf32> {ttir.name = "encoder_lin4.weight"}, %arg8: tensor<3xf32> {ttir.name = "encoder_lin4.bias"}, %arg9: tensor<3x12xf32> {ttir.name = "decoder_lin1.weight"}, %arg10: tensor<12xf32> {ttir.name = "decoder_lin1.bias"}, %arg11: tensor<12x64xf32> {ttir.name = "decoder_lin2.weight"}, %arg12: tensor<64xf32> {ttir.name = "decoder_lin2.bias"}, %arg13: tensor<64x128xf32> {ttir.name = "decoder_lin3.weight"}, %arg14: tensor<128xf32> {ttir.name = "decoder_lin3.bias"}, %arg15: tensor<128x784xf32> {ttir.name = "decoder_lin4.weight"}, %arg16: tensor<784xf32> {ttir.name = "decoder_lin4.bias"}) -> (tensor<1x784xf32> {ttir.name = "LinearAE.output_add_29"}) {
    %0 = ttir.empty() : tensor<1x128xf32>
    %1 = "ttir.matmul"(%arg0, %arg1, %0) : (tensor<1x784xf32>, tensor<784x128xf32>, tensor<1x128xf32>) -> tensor<1x128xf32>
    %2 = ttir.empty() : tensor<1x128xf32>
    %3 = "ttir.add"(%1, %arg2, %2) : (tensor<1x128xf32>, tensor<128xf32>, tensor<1x128xf32>) -> tensor<1x128xf32>
    %4 = ttir.empty() : tensor<1x128xf32>
    %5 = "ttir.relu"(%3, %4) : (tensor<1x128xf32>, tensor<1x128xf32>) -> tensor<1x128xf32>
    %6 = ttir.empty() : tensor<1x64xf32>
    %7 = "ttir.matmul"(%5, %arg3, %6) : (tensor<1x128xf32>, tensor<128x64xf32>, tensor<1x64xf32>) -> tensor<1x64xf32>
    %8 = ttir.empty() : tensor<1x64xf32>
    %9 = "ttir.add"(%7, %arg4, %8) : (tensor<1x64xf32>, tensor<64xf32>, tensor<1x64xf32>) -> tensor<1x64xf32>
    %10 = ttir.empty() : tensor<1x64xf32>
    %11 = "ttir.relu"(%9, %10) : (tensor<1x64xf32>, tensor<1x64xf32>) -> tensor<1x64xf32>
    %12 = ttir.empty() : tensor<1x12xf32>
    %13 = "ttir.matmul"(%11, %arg5, %12) : (tensor<1x64xf32>, tensor<64x12xf32>, tensor<1x12xf32>) -> tensor<1x12xf32>
    %14 = ttir.empty() : tensor<1x12xf32>
    %15 = "ttir.add"(%13, %arg6, %14) : (tensor<1x12xf32>, tensor<12xf32>, tensor<1x12xf32>) -> tensor<1x12xf32>
    %16 = ttir.empty() : tensor<1x12xf32>
    %17 = "ttir.relu"(%15, %16) : (tensor<1x12xf32>, tensor<1x12xf32>) -> tensor<1x12xf32>
    %18 = ttir.empty() : tensor<1x3xf32>
    %19 = "ttir.matmul"(%17, %arg7, %18) : (tensor<1x12xf32>, tensor<12x3xf32>, tensor<1x3xf32>) -> tensor<1x3xf32>
    %20 = ttir.empty() : tensor<1x3xf32>
    %21 = "ttir.add"(%19, %arg8, %20) : (tensor<1x3xf32>, tensor<3xf32>, tensor<1x3xf32>) -> tensor<1x3xf32>
    %22 = ttir.empty() : tensor<1x12xf32>
    %23 = "ttir.matmul"(%21, %arg9, %22) : (tensor<1x3xf32>, tensor<3x12xf32>, tensor<1x12xf32>) -> tensor<1x12xf32>
    %24 = ttir.empty() : tensor<1x12xf32>
    %25 = "ttir.add"(%23, %arg10, %24) : (tensor<1x12xf32>, tensor<12xf32>, tensor<1x12xf32>) -> tensor<1x12xf32>
    %26 = ttir.empty() : tensor<1x12xf32>
    %27 = "ttir.relu"(%25, %26) : (tensor<1x12xf32>, tensor<1x12xf32>) -> tensor<1x12xf32>
    %28 = ttir.empty() : tensor<1x64xf32>
    %29 = "ttir.matmul"(%27, %arg11, %28) : (tensor<1x12xf32>, tensor<12x64xf32>, tensor<1x64xf32>) -> tensor<1x64xf32>
    %30 = ttir.empty() : tensor<1x64xf32>
    %31 = "ttir.add"(%29, %arg12, %30) : (tensor<1x64xf32>, tensor<64xf32>, tensor<1x64xf32>) -> tensor<1x64xf32>
    %32 = ttir.empty() : tensor<1x64xf32>
    %33 = "ttir.relu"(%31, %32) : (tensor<1x64xf32>, tensor<1x64xf32>) -> tensor<1x64xf32>
    %34 = ttir.empty() : tensor<1x128xf32>
    %35 = "ttir.matmul"(%33, %arg13, %34) : (tensor<1x64xf32>, tensor<64x128xf32>, tensor<1x128xf32>) -> tensor<1x128xf32>
    %36 = ttir.empty() : tensor<1x128xf32>
    %37 = "ttir.add"(%35, %arg14, %36) : (tensor<1x128xf32>, tensor<128xf32>, tensor<1x128xf32>) -> tensor<1x128xf32>
    %38 = ttir.empty() : tensor<1x128xf32>
    %39 = "ttir.relu"(%37, %38) : (tensor<1x128xf32>, tensor<1x128xf32>) -> tensor<1x128xf32>
    %40 = ttir.empty() : tensor<1x784xf32>
    %41 = "ttir.matmul"(%39, %arg15, %40) : (tensor<1x128xf32>, tensor<128x784xf32>, tensor<1x784xf32>) -> tensor<1x784xf32>
    %42 = ttir.empty() : tensor<1x784xf32>
    %43 = "ttir.add"(%41, %arg16, %42) : (tensor<1x784xf32>, tensor<784xf32>, tensor<1x784xf32>) -> tensor<1x784xf32>
    return %43 : tensor<1x784xf32>
  }
}
