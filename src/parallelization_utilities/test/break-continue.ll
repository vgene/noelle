; ModuleID = 'examples/break-continue.c'
source_filename = "examples/break-continue.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@.str = private unnamed_addr constant [29 x i8] c"Reached %d 10s of iterations\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main(i32 %argc) #0 {
entry:
  %retval = alloca i32, align 4
  %argc.addr = alloca i32, align 4
  %break_stop = alloca i32, align 4
  %x = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  store i32 %argc, i32* %argc.addr, align 4
  %0 = load i32, i32* %argc.addr, align 4
  store i32 %0, i32* %break_stop, align 4
  store i32 0, i32* %x, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %1 = load i32, i32* %x, align 4, !note.noelle !2
  %cmp = icmp slt i32 %1, 100, !note.noelle !2
  br i1 %cmp, label %for.body, label %for.end, !note.noelle !2

for.body:                                         ; preds = %for.cond
  %2 = load i32, i32* %x, align 4, !note.noelle !2
  %rem = srem i32 %2, 10, !note.noelle !2
  %cmp1 = icmp eq i32 %rem, 0, !note.noelle !2
  br i1 %cmp1, label %if.then, label %if.end, !note.noelle !2

if.then:                                          ; preds = %for.body
  br label %for.inc, !note.noelle !2

if.end:                                           ; preds = %for.body
  %3 = load i32, i32* %x, align 4, !note.noelle !2
  %4 = load i32, i32* %break_stop, align 4, !note.noelle !2
  %cmp2 = icmp eq i32 %3, %4, !note.noelle !2
  br i1 %cmp2, label %if.then3, label %if.end4, !note.noelle !2

if.then3:                                         ; preds = %if.end
  br label %for.end, !note.noelle !2

if.end4:                                          ; preds = %if.end
  br label %for.inc, !note.noelle !2

for.inc:                                          ; preds = %if.end4, %if.then
  %5 = load i32, i32* %x, align 4, !note.noelle !2
  %inc = add nsw i32 %5, 1, !note.noelle !2
  store i32 %inc, i32* %x, align 4, !note.noelle !2
  br label %for.cond, !note.noelle !2

for.end:                                          ; preds = %if.then3, %for.cond
  %6 = load i32, i32* %x, align 4
  %div = sdiv i32 %6, 10
  store i32 %div, i32* %x, align 4
  %7 = load i32, i32* %x, align 4
  %call = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([29 x i8], [29 x i8]* @.str, i32 0, i32 0), i32 %7)
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
