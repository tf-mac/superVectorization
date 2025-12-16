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
  store i32 0, i32* %0, align 4
  store i32 0, i32* %1, align 4
  %3 = load i32, i32* %1, align 4
  %4 = call i32 @test(i32 noundef %3)
  store i32 %4, i32* %2, align 4
  %5 = load i32, i32* %2, align 4
  %6 = srem i32 %5, 16
  %7 = icmp ne i32 %6, 0
  br label %pred_block37

loop_header:                                      ; preds = %loop_latch
  br label %loop_latch

loop_latch:                                       ; preds = %loop_header, %true_block22
  %8 = load i32, i32* %1, align 4
  %9 = icmp slt i32 %8, 1000
  %10 = xor i1 %7, true
  %11 = and i1 %9, %10
  br i1 %11, label %loop_header, label %loop_exit

loop_exit:                                        ; preds = %loop_latch
  br label %pred_block37

pred_block:                                       ; No predecessors!
  br label %pred_block37

true_block:                                       ; No predecessors!
  br label %pred_block37

pred_block1:                                      ; No predecessors!
  br label %pred_block37

true_block2:                                      ; No predecessors!
  br label %pred_block37

pred_block3:                                      ; No predecessors!
  br label %pred_block37

true_block4:                                      ; No predecessors!
  %12 = load i32, i32* %1, align 4
  br label %pred_block37

pred_block5:                                      ; No predecessors!
  br label %pred_block37

true_block6:                                      ; No predecessors!
  br label %pred_block37

pred_block7:                                      ; No predecessors!
  br label %pred_block37

true_block8:                                      ; No predecessors!
  br label %pred_block37

pred_block9:                                      ; No predecessors!
  br label %pred_block37

true_block10:                                     ; No predecessors!
  %13 = add nsw i32 %12, 1
  br label %pred_block37

pred_block11:                                     ; No predecessors!
  br label %pred_block37

true_block12:                                     ; No predecessors!
  br label %pred_block37

pred_block13:                                     ; No predecessors!
  br label %pred_block37

true_block14:                                     ; No predecessors!
  br label %pred_block37

pred_block15:                                     ; No predecessors!
  br label %pred_block37

true_block16:                                     ; No predecessors!
  store i32 %13, i32* %1, align 4
  br label %pred_block37

pred_block17:                                     ; No predecessors!
  br label %pred_block37

true_block18:                                     ; No predecessors!
  br label %pred_block37

pred_block19:                                     ; No predecessors!
  br label %pred_block37

true_block20:                                     ; No predecessors!
  br label %pred_block37

pred_block21:                                     ; No predecessors!
  br label %pred_block37

true_block22:                                     ; No predecessors!
  br label %loop_latch

pred_block23:                                     ; No predecessors!
  br label %pred_block37

true_block24:                                     ; No predecessors!
  br label %pred_block37

pred_block25:                                     ; No predecessors!
  store i32 -1, i32* %0, align 4
  br label %pred_block37

true_block26:                                     ; No predecessors!
  br label %pred_block37

pred_block27:                                     ; No predecessors!
  br label %pred_block37

true_block28:                                     ; No predecessors!
  br label %pred_block37

pred_block29:                                     ; No predecessors!
  br label %pred_block37

true_block30:                                     ; No predecessors!
  br label %pred_block37

pred_block31:                                     ; No predecessors!
  br label %pred_block37

true_block32:                                     ; No predecessors!
  %14 = call i32 @test(i32 noundef 7)
  br label %pred_block37

pred_block33:                                     ; No predecessors!
  br label %pred_block37

true_block34:                                     ; No predecessors!
  br label %pred_block37

pred_block35:                                     ; No predecessors!
  br label %pred_block37

true_block36:                                     ; No predecessors!
  br label %pred_block37

pred_block37:                                     ; preds = %true_block38, %true_block36, %pred_block35, %true_block34, %pred_block33, %true_block32, %pred_block31, %true_block30, %pred_block29, %true_block28, %pred_block27, %true_block26, %pred_block25, %true_block24, %pred_block23, %pred_block21, %true_block20, %pred_block19, %true_block18, %pred_block17, %true_block16, %pred_block15, %true_block14, %pred_block13, %true_block12, %pred_block11, %true_block10, %pred_block9, %true_block8, %pred_block7, %true_block6, %pred_block5, %true_block4, %pred_block3, %true_block2, %pred_block1, %true_block, %pred_block, %loop_exit, %entry
  %15 = load i32, i32* %0, align 4
  ret i32 %15

true_block38:                                     ; No predecessors!
  store i32 %14, i32* %0, align 4
  br label %pred_block37
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
