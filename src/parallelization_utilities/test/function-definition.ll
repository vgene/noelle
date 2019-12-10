; ModuleID = 'examples/function-definition.c'
source_filename = "examples/function-definition.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [44 x i8] c"\0A%d: iteration is independent and unordered\00", align 1
@.str.1 = private unnamed_addr constant [3 x i8] c"\0A\0A\00", align 1
@.str.2 = private unnamed_addr constant [26 x i8] c"\0A%d: iteration is ordered\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @this_function_is_annotated() #0 {
entry:
  %x = alloca i32, align 4, !note.noelle !2
  %y = alloca i32, align 4, !note.noelle !2
  store i32 0, i32* %x, align 4, !note.noelle !2
  br label %for.cond, !note.noelle !2

for.cond:                                         ; preds = %for.inc, %entry
  %0 = load i32, i32* %x, align 4, !note.noelle !2
  %cmp = icmp slt i32 %0, 100, !note.noelle !2
  br i1 %cmp, label %for.body, label %for.end, !note.noelle !2

for.body:                                         ; preds = %for.cond
  %1 = load i32, i32* %x, align 4, !note.noelle !2
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([44 x i8], [44 x i8]* @.str, i32 0, i32 0), i32 %1), !note.noelle !2
  br label %for.inc, !note.noelle !2

for.inc:                                          ; preds = %for.body
  %2 = load i32, i32* %x, align 4, !note.noelle !2
  %inc = add nsw i32 %2, 1, !note.noelle !2
  store i32 %inc, i32* %x, align 4, !note.noelle !2
  br label %for.cond, !note.noelle !2

for.end:                                          ; preds = %for.cond
  %call1 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str.1, i32 0, i32 0)), !note.noelle !2
  store i32 0, i32* %y, align 4, !note.noelle !2
  br label %for.cond2, !note.noelle !2

for.cond2:                                        ; preds = %for.inc6, %for.end
  %3 = load i32, i32* %y, align 4, !note.noelle !5
  %cmp3 = icmp slt i32 %3, 100, !note.noelle !5
  br i1 %cmp3, label %for.body4, label %for.end8, !note.noelle !5

for.body4:                                        ; preds = %for.cond2
  %4 = load i32, i32* %y, align 4, !note.noelle !5
  %call5 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([26 x i8], [26 x i8]* @.str.2, i32 0, i32 0), i32 %4), !note.noelle !5
  br label %for.inc6, !note.noelle !5

for.inc6:                                         ; preds = %for.body4
  %5 = load i32, i32* %y, align 4, !note.noelle !5
  %inc7 = add nsw i32 %5, 1, !note.noelle !5
  store i32 %inc7, i32* %y, align 4, !note.noelle !5
  br label %for.cond2, !note.noelle !5

for.end8:                                         ; preds = %for.cond2
  ret void, !note.noelle !2
}

declare dso_local i32 @printf(i8*, ...) #1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local void @this_function_is_not() #0 {
entry:
  ret void
}

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  call void @this_function_is_annotated()
  call void @this_function_is_not()
  ret i32 0
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.0.0 "}
!2 = !{!3, !4}
!3 = !{!"independent", !"1"}
!4 = !{!"ordered", !"0"}
!5 = !{!3, !6}
!6 = !{!"ordered", !"1"}
