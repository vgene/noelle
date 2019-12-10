; ModuleID = 'examples/while-and-do-while.c'
source_filename = "examples/while-and-do-while.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [19 x i8] c"Back at %d yeeeeah\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  store i32 5, i32* %x, align 4
  br label %while.cond

while.cond:                                       ; preds = %if.end, %entry
  %0 = load i32, i32* %x, align 4, !note.noelle !2
  %cmp = icmp slt i32 %0, 10, !note.noelle !2
  br i1 %cmp, label %while.body, label %while.end, !note.noelle !2

while.body:                                       ; preds = %while.cond
  %1 = load i32, i32* %x, align 4, !note.noelle !2
  %inc = add nsw i32 %1, 1, !note.noelle !2
  store i32 %inc, i32* %x, align 4, !note.noelle !2
  %2 = load i32, i32* %x, align 4, !note.noelle !2
  %cmp1 = icmp eq i32 %2, 7, !note.noelle !2
  br i1 %cmp1, label %if.then, label %if.end, !note.noelle !2

if.then:                                          ; preds = %while.body
  br label %while.end, !note.noelle !2

if.end:                                           ; preds = %while.body
  br label %while.cond, !note.noelle !2

while.end:                                        ; preds = %if.then, %while.cond
  br label %do.body, !note.noelle !4

do.body:                                          ; preds = %do.cond, %while.end
  %3 = load i32, i32* %x, align 4, !note.noelle !4
  %dec = add nsw i32 %3, -1, !note.noelle !4
  store i32 %dec, i32* %x, align 4, !note.noelle !4
  %4 = load i32, i32* %x, align 4, !note.noelle !4
  %cmp2 = icmp eq i32 %4, 7, !note.noelle !4
  br i1 %cmp2, label %if.then3, label %if.end4, !note.noelle !4

if.then3:                                         ; preds = %do.body
  %5 = load i32, i32* %x, align 4, !note.noelle !4
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([19 x i8], [19 x i8]* @.str, i32 0, i32 0), i32 %5), !note.noelle !4
  br label %if.end4, !note.noelle !4

if.end4:                                          ; preds = %if.then3, %do.body
  br label %do.cond, !note.noelle !4

do.cond:                                          ; preds = %if.end4
  %6 = load i32, i32* %x, align 4, !note.noelle !4
  %cmp5 = icmp sgt i32 %6, 0, !note.noelle !4
  br i1 %cmp5, label %do.body, label %do.end, !note.noelle !4

do.end:                                           ; preds = %do.cond
  ret i32 0
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
!4 = !{!5, !6}
!5 = !{!"hello", !"world"}
!6 = !{!"ordered", !"0"}
