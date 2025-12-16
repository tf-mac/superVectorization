; ModuleID = './test.c.ll'
source_filename = "./test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @test(i32 noundef %0) #0 {
entry:
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  store i32 %0, i32* %1, align 4
  %3 = load i32, i32* %1, align 4
  %4 = mul nsw i32 %3, 8
  store i32 %4, i32* %2, align 4
  %5 = load i32, i32* %2, align 4
  ret i32 %5
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
entry:
  %0 = alloca i32, align 4
  %1 = alloca i32, align 4
  %2 = alloca i32, align 4
  %3 = alloca i32, align 4
  %4 = alloca i32, align 4
  store i32 0, i32* %2, align 4
  store i32 0, i32* %3, align 4
  br label %loop_header

loop_header:                                      ; preds = %pred_block65, %entry
  %5 = load i32, i32* %3, align 4
  %6 = icmp slt i32 %5, 1000000000
  br i1 %6, label %pred_block, label %true_block

loop_exit:                                        ; preds = %pred_block63, %pred_block55, %pred_block47, %pred_block31, %pred_block23, %true_block66
  %7 = call i32 @test(i32 noundef 7)
  ret i32 %7

pred_block:                                       ; preds = %loop_header
  %8 = load i32, i32* %3, align 4
  br i1 %6, label %pred_block1, label %true_block2

true_block:                                       ; preds = %loop_header
  br i1 %6, label %pred_block1, label %true_block2

pred_block1:                                      ; preds = %true_block, %pred_block
  %9 = call i32 @test(i32 noundef %8)
  br i1 %6, label %pred_block3, label %true_block4

true_block2:                                      ; preds = %true_block, %pred_block
  br i1 %6, label %pred_block3, label %true_block4

pred_block3:                                      ; preds = %true_block2, %pred_block1
  store i32 %9, i32* %4, align 4
  br i1 %6, label %pred_block5, label %true_block6

true_block4:                                      ; preds = %true_block2, %pred_block1
  br i1 %6, label %pred_block5, label %true_block6

pred_block5:                                      ; preds = %true_block4, %pred_block3
  %10 = load i32, i32* %4, align 4
  br i1 %6, label %pred_block7, label %true_block8

true_block6:                                      ; preds = %true_block4, %pred_block3
  br i1 %6, label %pred_block7, label %true_block8

pred_block7:                                      ; preds = %true_block6, %pred_block5
  %11 = icmp slt i32 %10, 0
  br i1 %11, label %pred_block9, label %true_block10

true_block8:                                      ; preds = %true_block6, %pred_block5
  br i1 %11, label %pred_block9, label %true_block10

pred_block9:                                      ; preds = %true_block8, %pred_block7
  %12 = load i32, i32* %3, align 4
  br i1 %11, label %pred_block11, label %true_block12

true_block10:                                     ; preds = %true_block8, %pred_block7
  br i1 %11, label %pred_block11, label %true_block12

pred_block11:                                     ; preds = %true_block10, %pred_block9
  %13 = load i32, i32* %4, align 4
  br i1 %11, label %pred_block13, label %true_block14

true_block12:                                     ; preds = %true_block10, %pred_block9
  br i1 %11, label %pred_block13, label %true_block14

pred_block13:                                     ; preds = %true_block12, %pred_block11
  %14 = add nsw i32 %13, %12
  br i1 %11, label %pred_block15, label %true_block16

true_block14:                                     ; preds = %true_block12, %pred_block11
  br i1 %11, label %pred_block15, label %true_block16

pred_block15:                                     ; preds = %true_block14, %pred_block13
  store i32 %14, i32* %4, align 4
  br i1 %11, label %pred_block19, label %true_block20

true_block16:                                     ; preds = %true_block14, %pred_block13
  br i1 %11, label %pred_block19, label %true_block20

pred_block17:                                     ; preds = %true_block24
  %15 = load i32, i32* %4, align 4
  br i1 %11, label %pred_block27, label %true_block28

pred_block19:                                     ; preds = %true_block16, %pred_block15
  br i1 %11, label %pred_block23, label %true_block24

true_block20:                                     ; preds = %true_block16, %pred_block15
  br i1 %11, label %pred_block23, label %true_block24

pred_block23:                                     ; preds = %true_block20, %pred_block19
  br label %loop_exit

true_block24:                                     ; preds = %true_block20, %pred_block19
  br label %pred_block17

pred_block25:                                     ; preds = %true_block32
  %16 = icmp slt i32 %15, 0
  br i1 %16, label %pred_block33, label %true_block34

pred_block27:                                     ; preds = %pred_block17
  br i1 %11, label %pred_block31, label %true_block32

true_block28:                                     ; preds = %pred_block17
  br i1 %11, label %pred_block31, label %true_block32

pred_block31:                                     ; preds = %true_block28, %pred_block27
  br label %loop_exit

true_block32:                                     ; preds = %true_block28, %pred_block27
  br label %pred_block25

pred_block33:                                     ; preds = %pred_block25
  %17 = load i32, i32* %3, align 4
  br i1 %16, label %pred_block35, label %true_block36

true_block34:                                     ; preds = %pred_block25
  br i1 %16, label %pred_block35, label %true_block36

pred_block35:                                     ; preds = %true_block34, %pred_block33
  %18 = load i32, i32* %4, align 4
  br i1 %16, label %pred_block37, label %true_block38

true_block36:                                     ; preds = %true_block34, %pred_block33
  br i1 %16, label %pred_block37, label %true_block38

pred_block37:                                     ; preds = %true_block36, %pred_block35
  %19 = add nsw i32 %18, %17
  br i1 %16, label %pred_block39, label %true_block40

true_block38:                                     ; preds = %true_block36, %pred_block35
  br i1 %16, label %pred_block39, label %true_block40

pred_block39:                                     ; preds = %true_block38, %pred_block37
  store i32 %19, i32* %4, align 4
  br i1 %16, label %pred_block43, label %true_block44

true_block40:                                     ; preds = %true_block38, %pred_block37
  br i1 %16, label %pred_block43, label %true_block44

pred_block41:                                     ; preds = %true_block48
  %20 = load i32, i32* %3, align 4
  br i1 %16, label %pred_block51, label %true_block52

pred_block43:                                     ; preds = %true_block40, %pred_block39
  br i1 %16, label %pred_block47, label %true_block48

true_block44:                                     ; preds = %true_block40, %pred_block39
  br i1 %16, label %pred_block47, label %true_block48

pred_block47:                                     ; preds = %true_block44, %pred_block43
  br label %loop_exit

true_block48:                                     ; preds = %true_block44, %pred_block43
  br label %pred_block41

pred_block49:                                     ; preds = %true_block56
  %21 = add nsw i32 %20, 1
  br i1 %16, label %pred_block59, label %true_block60

pred_block51:                                     ; preds = %pred_block41
  br i1 %16, label %pred_block55, label %true_block56

true_block52:                                     ; preds = %pred_block41
  br i1 %16, label %pred_block55, label %true_block56

pred_block55:                                     ; preds = %true_block52, %pred_block51
  br label %loop_exit

true_block56:                                     ; preds = %true_block52, %pred_block51
  br label %pred_block49

pred_block57:                                     ; preds = %true_block64
  store i32 %21, i32* %3, align 4
  br i1 %6, label %pred_block65, label %true_block66

pred_block59:                                     ; preds = %pred_block49
  br i1 %16, label %pred_block63, label %true_block64

true_block60:                                     ; preds = %pred_block49
  br i1 %16, label %pred_block63, label %true_block64

pred_block63:                                     ; preds = %true_block60, %pred_block59
  br label %loop_exit

true_block64:                                     ; preds = %true_block60, %pred_block59
  br label %pred_block57

pred_block65:                                     ; preds = %pred_block57
  br label %loop_header

true_block66:                                     ; preds = %pred_block57
  br label %loop_exit
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 7, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 1}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 14.0.0-1ubuntu1.1"}
