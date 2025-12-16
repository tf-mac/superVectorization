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
  br label %loop_exit

loop_header:                                      ; No predecessors!
  %3 = load i32, i32* %1, align 4
  %4 = call i32 @test(i32 noundef %3)
  store i32 %4, i32* %2, align 4
  %5 = load i32, i32* %2, align 4
  %6 = icmp slt i32 %5, 0
  %7 = load i32, i32* %1, align 4
  %8 = add nsw i32 %7, 1
  store i32 %8, i32* %1, align 4
  br label %loop_latch

loop_latch:                                       ; preds = %loop_header
  br label %loop_exit

loop_exit:                                        ; preds = %loop_latch, %entry
  store i32 -1, i32* %0, align 4
  %9 = call i32 @test(i32 noundef 7)
  store i32 %9, i32* %0, align 4
  %10 = load i32, i32* %0, align 4
  ret i32 %10
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
