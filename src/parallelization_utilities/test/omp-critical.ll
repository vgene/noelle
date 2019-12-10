; ModuleID = 'examples/omp-critical.c'
source_filename = "examples/omp-critical.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [12 x i8] c"%d, %d, %d\0A\00", align 1
@.str.1 = private unnamed_addr constant [20 x i8] c"on %d hit %d for %d\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @block_counter() #0 {
entry:
  %COUNT = alloca i32, align 4
  %x = alloca i32, align 4
  %y = alloca i32, align 4
  %z = alloca i32, align 4
  store i32 0, i32* %COUNT, align 4
  %0 = load i32, i32* %COUNT, align 4, !note.noelle !2
  store i32 %0, i32* %x, align 4, !note.noelle !2
  %1 = load i32, i32* %COUNT, align 4, !note.noelle !2
  %inc = add nsw i32 %1, 1, !note.noelle !2
  store i32 %inc, i32* %COUNT, align 4, !note.noelle !2
  %2 = load i32, i32* %COUNT, align 4, !note.noelle !2
  %inc1 = add nsw i32 %2, 1, !note.noelle !2
  store i32 %inc1, i32* %COUNT, align 4, !note.noelle !2
  %3 = load i32, i32* %COUNT, align 4, !note.noelle !2
  %inc2 = add nsw i32 %3, 1, !note.noelle !2
  store i32 %inc2, i32* %COUNT, align 4, !note.noelle !2
  %4 = load i32, i32* %x, align 4, !note.noelle !4
  %add = add nsw i32 %4, 1, !note.noelle !4
  store i32 %add, i32* %y, align 4, !note.noelle !4
  %5 = load i32, i32* %y, align 4, !note.noelle !4
  %6 = load i32, i32* %x, align 4, !note.noelle !4
  %mul = mul nsw i32 %5, %6, !note.noelle !4
  store i32 %mul, i32* %z, align 4, !note.noelle !4
  %7 = load i32, i32* %x, align 4, !note.noelle !4
  %8 = load i32, i32* %y, align 4, !note.noelle !4
  %9 = load i32, i32* %z, align 4, !note.noelle !4
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([12 x i8], [12 x i8]* @.str, i32 0, i32 0), i32 %7, i32 %8, i32 %9), !note.noelle !4
  ret void
}

declare dso_local i32 @printf(i8*, ...) #1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @loop_counter() #0 {
entry:
  %NUMS = alloca [10 x i32], align 16
  %i = alloca i32, align 4
  %count_modulo_10 = alloca i32, align 4
  %half = alloca i32, align 4
  %0 = bitcast [10 x i32]* %NUMS to i8*
  call void @llvm.memset.p0i8.i64(i8* align 16 %0, i8 0, i64 40, i1 false)
  store i32 0, i32* %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %1 = load i32, i32* %i, align 4, !note.noelle !6
  %cmp = icmp slt i32 %1, 100, !note.noelle !6
  br i1 %cmp, label %for.body, label %for.end, !note.noelle !6

for.body:                                         ; preds = %for.cond
  %2 = load i32, i32* %i, align 4, !note.noelle !8
  %rem = srem i32 %2, 10, !note.noelle !8
  %idxprom = sext i32 %rem to i64, !note.noelle !8
  %arrayidx = getelementptr inbounds [10 x i32], [10 x i32]* %NUMS, i64 0, i64 %idxprom, !note.noelle !8
  %3 = load i32, i32* %arrayidx, align 4, !note.noelle !8
  %inc = add nsw i32 %3, 1, !note.noelle !8
  store i32 %inc, i32* %arrayidx, align 4, !note.noelle !8
  %4 = load i32, i32* %i, align 4, !note.noelle !8
  %rem1 = srem i32 %4, 10, !note.noelle !8
  %idxprom2 = sext i32 %rem1 to i64, !note.noelle !8
  %arrayidx3 = getelementptr inbounds [10 x i32], [10 x i32]* %NUMS, i64 0, i64 %idxprom2, !note.noelle !8
  %5 = load i32, i32* %arrayidx3, align 4, !note.noelle !8
  store i32 %5, i32* %count_modulo_10, align 4, !note.noelle !8
  %6 = load i32, i32* %count_modulo_10, align 4, !note.noelle !6
  %div = sdiv i32 %6, 2, !note.noelle !6
  store i32 %div, i32* %half, align 4, !note.noelle !6
  %7 = load i32, i32* %half, align 4, !note.noelle !6
  %cmp4 = icmp sgt i32 %7, 5, !note.noelle !6
  br i1 %cmp4, label %if.then, label %if.end, !note.noelle !6

if.then:                                          ; preds = %for.body
  %8 = load i32, i32* %i, align 4, !note.noelle !6
  %9 = load i32, i32* %count_modulo_10, align 4, !note.noelle !6
  %10 = load i32, i32* %i, align 4, !note.noelle !6
  %rem5 = srem i32 %10, 10, !note.noelle !6
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([20 x i8], [20 x i8]* @.str.1, i32 0, i32 0), i32 %8, i32 %9, i32 %rem5), !note.noelle !6
  %11 = load i32, i32* %i, align 4, !note.noelle !10
  %rem6 = srem i32 %11, 10, !note.noelle !10
  %idxprom7 = sext i32 %rem6 to i64, !note.noelle !10
  %arrayidx8 = getelementptr inbounds [10 x i32], [10 x i32]* %NUMS, i64 0, i64 %idxprom7, !note.noelle !10
  store i32 0, i32* %arrayidx8, align 4, !note.noelle !10
  br label %if.end, !note.noelle !6

if.end:                                           ; preds = %if.then, %for.body
  br label %for.inc, !note.noelle !6

for.inc:                                          ; preds = %if.end
  %12 = load i32, i32* %i, align 4, !note.noelle !6
  %inc9 = add nsw i32 %12, 1, !note.noelle !6
  store i32 %inc9, i32* %i, align 4, !note.noelle !6
  br label %for.cond, !note.noelle !6

for.end:                                          ; preds = %for.cond
  ret void
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1) #2

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  call void @block_counter()
  call void @loop_counter()
  ret i32 0
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.0.0 "}
!2 = !{!3}
!3 = !{!"independent", !"0"}
!4 = !{!5}
!5 = !{!"independent", !"1"}
!6 = !{!5, !7}
!7 = !{!"ordered", !"1"}
!8 = !{!3, !9}
!9 = !{!"ordered", !"0"}
!10 = !{!3, !7}
