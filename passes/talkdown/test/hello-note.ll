; ModuleID = 'examples/hello-note.c'
source_filename = "examples/hello-note.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [13 x i8] c"hello, world\00", align 1
@.str.1 = private unnamed_addr constant [3 x i8] c"%s\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %i = alloca i32, align 4
  %x = alloca i32, align 4
  %str = alloca i8*, align 8
  store i32 0, i32* %retval, align 4
  store i32 0, i32* %i, align 4
  br label %for.cond, !note.noelle !2

for.cond:                                         ; preds = %for.inc, %entry
  br label %for.inc, !note.noelle !2

for.inc:                                          ; preds = %for.cond
  %0 = load i32, i32* %i, align 4, !note.noelle !2
  %inc = add nsw i32 %0, 1, !note.noelle !2
  store i32 %inc, i32* %i, align 4, !note.noelle !2
  br label %for.cond, !note.noelle !2

hi:                                               ; No predecessors!
  store i32 4, i32* %x, align 4, !note.noelle !2
  %1 = load i32, i32* %x, align 4, !note.noelle !2
  %inc1 = add nsw i32 %1, 1, !note.noelle !2
  store i32 %inc1, i32* %x, align 4, !note.noelle !2
  store i8* getelementptr inbounds ([13 x i8], [13 x i8]* @.str, i32 0, i32 0), i8** %str, align 8, !note.noelle !2
  %2 = load i8*, i8** %str, align 8, !note.noelle !2
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str.1, i32 0, i32 0), i8* %2), !note.noelle !2
  %3 = load i32, i32* %x, align 4, !note.noelle !2
  %sub = sub nsw i32 %3, 5, !note.noelle !2
  ret i32 %sub
}

declare dso_local i32 @printf(i8*, ...) #1

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 7.0.0 "}
!2 = !{!3}
!3 = !{!"independent", !"1"}
