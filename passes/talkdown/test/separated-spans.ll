; ModuleID = 'examples/separated-spans.c'
source_filename = "examples/separated-spans.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [3 x i8] c"%d\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  store i32 5, i32* %x, align 4, !note.noelle !2
  %0 = load i32, i32* %x, align 4, !note.noelle !2
  %inc = add nsw i32 %0, 1, !note.noelle !2
  store i32 %inc, i32* %x, align 4, !note.noelle !2
  %1 = load i32, i32* %x, align 4, !note.noelle !4
  %dec = add nsw i32 %1, -1, !note.noelle !4
  store i32 %dec, i32* %x, align 4, !note.noelle !4
  %2 = load i32, i32* %x, align 4, !note.noelle !4
  %dec1 = add nsw i32 %2, -1, !note.noelle !4
  store i32 %dec1, i32* %x, align 4, !note.noelle !4
  %3 = load i32, i32* %x, align 4, !note.noelle !4
  %dec2 = add nsw i32 %3, -1, !note.noelle !4
  store i32 %dec2, i32* %x, align 4, !note.noelle !4
  br label %for.cond, !note.noelle !2

for.cond:                                         ; preds = %for.inc, %entry
  %4 = load i32, i32* %x, align 4, !note.noelle !2
  %cmp = icmp slt i32 %4, 5, !note.noelle !2
  br i1 %cmp, label %for.body, label %for.end, !note.noelle !2

for.body:                                         ; preds = %for.cond
  br label %for.inc, !note.noelle !2

for.inc:                                          ; preds = %for.body
  %5 = load i32, i32* %x, align 4, !note.noelle !2
  %inc3 = add nsw i32 %5, 1, !note.noelle !2
  store i32 %inc3, i32* %x, align 4, !note.noelle !2
  br label %for.cond, !note.noelle !2

for.end:                                          ; preds = %for.cond
  %6 = load i32, i32* %x, align 4, !note.noelle !6
  %dec4 = add nsw i32 %6, -1, !note.noelle !6
  store i32 %dec4, i32* %x, align 4, !note.noelle !6
  %7 = load i32, i32* %x, align 4, !note.noelle !6
  %dec5 = add nsw i32 %7, -1, !note.noelle !6
  store i32 %dec5, i32* %x, align 4, !note.noelle !6
  %8 = load i32, i32* %x, align 4, !note.noelle !6
  %dec6 = add nsw i32 %8, -1, !note.noelle !6
  store i32 %dec6, i32* %x, align 4, !note.noelle !6
  %9 = load i32, i32* %x, align 4, !note.noelle !2
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i32 0, i32 0), i32 %9), !note.noelle !2
  %10 = load i32, i32* %retval, align 4
  ret i32 %10
}

declare dso_local i32 @printf(i8*, ...) #1

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.0.0 "}
!2 = !{!3}
!3 = !{!"independent", !"0"}
!4 = !{!3, !5}
!5 = !{!"ordered", !"1"}
!6 = !{!3, !7}
!7 = !{!"ordered", !"0"}
