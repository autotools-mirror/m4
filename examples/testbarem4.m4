_ Tests for barem4.m4, July 2025, distributed under Creative
_ Commons Share-alike license.  -Doug McIlroy
_
_ Usage: m4 barem4.m4 testbarem4.m4
_
_  test(`x',`y') tests whether expressions x and y have identical values.
_  If not, print x != y, otherwise do nothing.
_  Uses the builtin ifelse to compare (nearly) arbitrary strings.
_
define(test,`ifelse($1,$2,`',``$1' != `$2'
')')_
_
_
`t0: here a deliberate error should print just "one != two"'
_
test(`one',`two')_
`From here on test descriptions will be printed.
Some tests will announce expected output. Otherwise
the only test results printed will be errors.'
_
define(`five', succ(four))_
define(`ten',  (0,five))_
define(`fifteen', add(five,ten))_
define(`twenty',  (0,ten))_
test(`ten',(0,(1,(0,(1,())))))_
_
_
t1: i+j-i = j, 0<=i,j<=10
_
define(`t1',  `test(`dim(add($1,$2),$1)',`$2')')_
define(`_t1', `forA(zero,ten,`t1',$1)')_
for(zero,ten, `_t1')_
_
_
t2:(i-j)(i+j) = i^2-j^2, 0<=j<=i<=3
_
define(`t2',_
__`test(`mul(dim($1,$2),add($1,$2))',`dim(square($1),square($2))')')_
define(`_t2', `forA(zero,$1,`t2',$1)')_
for(zero,three,`_t2')_
_
_
t3: (n/d)*d + n%d = n, 0<=n<=15, 1<=d<=20
_
define(`t3',  `test(`add(mul(div($1,$2),$2),mod($1,$2))',`$1')')_
define(`t3',  `test(mod($1,$2), trim(mod($1,$2)))')_
define(`_t3', `forA(one,twenty,`t3',$1)')_
for(zero,fifteen, `_t3')_
_
_
t4: a^(n-1) = a^n/a,  1<=a<=4, 1<=n<=5
_
define(`t4',  `test(`pow($1,dim($2,one))',`div(pow($1,$2),$1)')')_
define(`_t4', `forA(one,five,`t4',$1)')_
for(one,four,`_t4')_
_
_
t5: numeric comparisons 0<=i,j<=5
_
define(`t5',_
__`test(`lt($1,$2)',`not(ge($1,$2))')'_
__`test(`le($1,$2)',`not(gt($1,$2))')'_
__`test(`et($1,$2)',`not(ne($1,$2))')'_
__`test(`lt($1,$2)',`gt($2,$1)')'_
__`test(`le($1,$2)',`ge($2,$1)')'_
__`test(`et($1,$2)',`et($2,$1)')'_
__`test(`ne($1,$2)',`ne($2,$1)')')_
define(`_t5', `forA(zero,five,`t5',$1)')_
for(zero,five,`_t5')_
_
_
t6:  octal addition table should be printed in base 2
_
define(`seven',  add(three,four))_
define(`table',  `for(zero,seven,`row')')_
define(`row',    `forA(zero,seven,`cell',$1)newline()')_
define(`cell',   `base2(add($1,$2))tab()')_
define(`tab',    `	')_
define(`newline',`
')_
table()_
_
_
`t7: alphanumeric comparisons, map, fold, zip'
_
_ ordered(<c>,<list>) tests whether all characters that precede
_ <c> in a list of characters compare low to <c> and all
_ characters after <c> compare high. if <c> is absent, all
_ characters are deemed to precede it. If <c> is duplicated,
_ the second instance is deemed to come after the first. `ttab'
_ (transition table) tells whether adjacent judgements conform
_ to the specification.
_
switch2of2(`ttab',`$1',`$2')_
case(`ttab_LT_LT', `T')_
case(`ttab_LT_EQ', `T')_
case(`ttab_LT_GT', `F')_
case(`ttab_EQ_LT', `F')_
case(`ttab_EQ_EQ', `F')_
case(`ttab_EQ_GT', `T')_
case(`ttab_GT_LT', `F')_
case(`ttab_GT_EQ', `F')_
case(`ttab_GT_GT', `T')_
_
define(`ordered',_
__`define(`ordcmp', `cmp($'`1,$1)')'_
__`define(`judged', map(`ordcmp',$2))'_
__`foldl(`and',T,zip(`ttab',(LT,judged),judged))')_
_
test(`ordered(`0',`alphabet')',T)_
test(`ordered(`L',`alphabet')',T)_
test(`ordered(`z',`alphabet')',T)_
test(`ordered(2,(3,(2,(3,()))))',F)_
test(`ordered(2,(1,(1,(2,()))))',T)_
test(`ordered(2,(1,(1,(1,()))))',T)_
test(`ordered(2,(1,(2,(2,()))))',F)_
test(`ordered(2,(1,(2,(1,()))))',F)_
_
_
`p1-p5 are same as t1-t5 with zero-padded operands.'
_ Helper functions _t1-_t5 are unchanged
_
define(`pad',`rev((0,(0,rev($1))))')_
_
define(`test', `if(et($1,$2), `', ``$1' != `$2'
')')_
_
_
p1: i+j-i = j, 0<=i,j<=10
_
define(`t1',  `test(`dim(add(pad($1),pad($2)),pad($1))',`$2')')_
for(zero,ten, `_t1')_
_
_
p2:(i-j)(i+j) = i^2-j^2, 0<=j<=i<=3
_
define(`t2', `test(`mul(dim(pad($1),$2),add(pad($1),pad($2)))',`dim(square($1),square($2))')')_
for(zero,three,`_t2')_
_
_
p3: (n/d)*d + n%d = n, 0<=n<=15, 1<=d<=20
_
define(`t3',  `test(`add(mul(div(pad($1),pad($2)),pad($2)),mod(pad($1),pad($2)))',`$1')')_
for(zero,fifteen, `_t3')_
_
_
p4: a^(n-1) = a^n/a,  1<=a<=4, 1<=n<=5
_
define(`t4',  `test(`pow(pad($1),dim(pad($2),one))',`div(pow($1,$2),$1)')')_
for(one,four,`_t4')_
_
_
p5: numeric comparisons 0<=i,j<=5
_
define(`test', `if(eq($1,$2), `', ``$1' != `$2'
')')_
_
define(`t5',_
__`test(`lt($1,pad($2))',`not(ge($1,pad($2)))')'_
__`test(`le($1,pad($2))',`not(gt($1,pad($2)))')'_
__`test(`et($1,pad($2))',`not(ne($1,pad($2)))')'_
__`test(`lt($1,pad($2))',`gt(pad($2),$1)')'_
__`test(`le($1,pad($2))',`ge(pad($2),$1)')'_
__`test(`et($1,pad($2))',`et(pad($2),$1)')'_
__`test(`ne($1,pad($2))',`ne(pad($2),$1)')')_
define(`_t5', `forA(zero,five,`t5',$1)')_
for(zero,five,`_t5')_
