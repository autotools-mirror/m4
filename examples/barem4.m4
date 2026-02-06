define(`_',`dnl')_  unobtrusive dnl, used for readability
define(`__',`')_    indentation macros
define(`____',`')_     think of them as spaces that can't
define(`______',`')_   stick around and get in the way
_
_ This program is distributed under a Creative Commons
_ Share-alike license. An accompanying suite of tests
_ may be run thus: "m4 barem4.m4 testbarem4.m4".
_
_
_        Pure macros as a programming language
_
_ m4 is Turing complete even when stripped to the bare minimum
_ of one builtin: `define'. This is not news; Christopher
_ Strachey demonstrated it in his ancestral GPM, described in
_ "A general- purpose macrogenerator", The Computer Journal 8
_ (1965) 225-241.
_
_ This m4 program more fully illustrates universality by
_ building familiar programming capabilities: unlimited
_ precision integer arithmetic, boolean algebra, conditional
_ execution, case-switching, and some higher-level operators
_ from functional programming. In support of these normal
_ facilities, however, the program exploits some unusual
_ programming idioms:
_
_ 1. Case-switching via macro names constructed on the fly.
_ 2. Equality testing by redefining macros.
_ 3. Representing data structures by nested parenthesized lists.
_ 4. Using macros as associative memory.
_ 5. Inserting nested parameter symbols on the fly.
_
_ Idioms 2 and 5 are "reflective": the program writes code
_ for itself.
_
_ Note. There is a `dnl' on every line of this program. The
_ indulgence of using this builtin is logically unnecessary.
_ Stripped of `dnl's and indentation macros, the program would
_ consist entirely of `define's with no comments or line breaks.
_ It would be less than 1/4 the current size and would yield the
_ same results, but would be inscrutable.
_
_ In examples of stepwise macro expansion, all occurrences of
_ `dnl' and indentation have been elided.
_
_ For consistency, we quote all arguments of `define' unless
_ they contain macro calls that must be expanded at the
_ time of definition.
_
_        Case switch (Idiom 1)
_
_ An essential feature of programming is conditional execution.
_ Suppose there are predicates that yield `T' or `F' for true
_ or false. A conditional switch of the form
_
_    if(<T or F>, `<T action>', `<F action>')
_
_ constructs calls for `if_T', or `if_F' that select the
_ appropriate action:
_
define(`if', `if_$1(`$2', `$3')')_
define(`if_T', `$1')_
define(`if_F', `$2')_
_
_ Example
_
_    if(T,`yes',`no') => if_T(`yes',`no') => yes
_    if(F,`yes',`no') => if_F(`yes',`no') => no
_
_ (An arrow => signifies a single macro-expansion step.)
_
_ Again for consistency, we quote the second and third arguments
_ of `if' unless necessity dictates otherwise.
_
_ Basic Boolean functions are easy to define in terms of `if'.
_
define(`not', `if(`$1', `F',  `T')')_
define(`and', `if(`$1', `$2', `F')')_
define(`or',  `if(`$1', `T',  `$2')')_
define(`nor', `not(or($1,$2))')_
_
_        List representation (Idiom 3)
_
_ In order to provide access to individual members, sequences
_ of data values may be represented as right-associated lists.
_ In particular, binary integers may be represented in this
_ manner:
_
_    (1,(0,(1,(1,()))))
_
_ To facilitate arithmetic algorithms, the presentation is
_ little-endian. In customary base-2 notation the example
_ becomes 1101 (decimal 13). An empty list `()' acts as a
_ terminator. Zero is normally represented by the empty
_ list. Other values normally end in (1,()). Although
_ insignificant 0 bits after the last 1 in a list are
_ generally harmless, algorithms should not gratuitously
_ create them.
_
_ Data may be saved under arbitrary names (Idiom 4). Here
_ we give more readable names for some small numbers.
_ Quotes have been omitted from the definition for `two',
_ so that `one' gets expanded at definition time rather
_ than at each call of `two', paying a small amount of
_ space to save time at each call.
_
define(`zero', `()')_
define(`one',  `(1,())')_
define(`two',  (0,one))_
_
_ Individual list elements may be accessed by a pun, in which
_ the outer parentheses of a list are taken to be the
_ argument-list delimiters in a macro call. Two basic macros
_ use this scheme to extract "head" and "tail" parts, <h>
_ and <t>, from a nonempty list:
_
_    head((<h>,<t>)) ==> <h>
_    tail((<h>,<t>)) ==> <t>
_
_ (A long arrow ==> signifies more than one macro expansion.)
_
define(`head',  `_head$1')_
define(`_head', `$1')_
_
define(`tail',  `_tail$1')_
define(`_tail', `$2')_
_
_ Examples
_
_    head(two) => head((0,(1,()))) => _head(0,(1,())) => 0
_    tail(two) => tail((0,(1,()))) => _tail(0,(1,())) => (1,())
_
_ According to the rules of m4, `head' and `tail' also work on
_ the empty list, `()', in which case both functions yield an
_ empty string, `'. This behavior will be exploited.
_
_        Testing equality of primitive values (Idiom 2)
_
_ The `if' macro checks which of two possible values (`T' or `F')
_ appears as its first argument. More trickery is required to
_ check equality of arbitrary primitive values--sequences of
_ characters that can occur in macro names, e.g; `Abc', `3_2_1'.
_ Following Lisp, we call such a sequence an "atom".
_
_ We first define the outcome of the test of the given atoms to
_ be F. Then we define the test of one of the atoms against
_ itself to be T. If the two atoms are the same, the second
_ definition replaces the first. Finally we call the first.
_
define(`eq',_
__`define(`eq_$1_$2', `F')'_
__`define(`eq_$1_$1', `T')'_
__`eq_$1_$2')_
_
_ Examples
_
_ eq(a,b) => define(`eq_a_b',`F')define(`eq_a_a',`T')eq_a_b
_ ==> eq_a_b => F
_ eq(a,a) => define(`eq_a_a',`F')define(`eq_a_a',`T')eq_a_a
_ ==> eq_a_a => T
_
_ If the T or F result of `eq' or other boolean-valued macro
_ is to be juxtaposed with succeeding text, care must be
_ taken to prevent unwanted catenation. An intervening
_ quoted empty string suffices to make the separation.
_
_    eq(a,b)`'eq(c,d) ==> FF
_    eq(a,b)eq(c,d) ==> Feq(c,d)
_
_ Unfortunately the temporary definitions stick around as
_ "ghosts" that waste memory. We could relax our notion of
_ purity and use the builtin macro `undefine' to clean up.
_
_ A test for the empty list:
_
define(`isempty', `eq(head($1), `')')_
_
_ Equality of lists of atoms can be tested by `eql',
_ which uses `eq' recursively. The indentation in this
_ definition and others like it is intended to imitate a
_ sequence of else-less if's.
_
define(`eql',_
__`if(isempty($1), `isempty($2)',_
__`if(eq(head($1),head($2)), `eql(tail($1),tail($2))',_
__`F')')')_
_
_ Example
_
_    eql((W,(H,(O,()))), (W,(H,(O,(M,())))))
_    =>  if(isempty((W,(H,(O,())))), ...)
_    =>  if(eq(W,`'), ...)
_    ==> if(eq(W,W), `eql((H,(O,())),(H,(O,(M())))), ...)
_    ==> if(isempty(()), `isempty((M,())),, ...)
_    ==> F
_
_ Here we use `eq' to define a predicate for testing whether
_ a number, possibly with insignificant zero digits, is zero.
_
define(`iszero',_
__`if(isempty($1), `T',_
__`if(eq(head($1),0), `iszero(tail($1))',_
__`F')')')_
_
_ Example
_
_    iszero(())
_    ==> if(eq(,),`T',`if(...)')
_    ==> if(T,`T',`if(...)' ==> T
_
_    iszero((0,(1,())))
_    ==> if(eq(0,),...,`if(eq(head((0,(1,()))),0),...)')
_    ==> iszero(tail(0,(1,())))
_    ==> iszero((1,())) ==> F
_
_ Basic to all arithmeic is the successor function.
_
define(`succ',_
__`if(iszero($1), one,_
__`if(eq(head($1),0), `(1,tail($1))',_
__`(0,succ(tail($1)))')')')_
_
define(`three', succ(two))_
define(`four',  succ(three))_
_
_ In the definition of `succ', the behavior for each of the
_ three possible values of the head digit of the argument
_ (empty, `0' and `1') is presented in a different context:
_ as the first branch of an `if', as the first branch of
_ a nested `if', and as the second branch of a nested `if'.
_ Parentheses become hard to match. It would be cleaner to
_ use a 3-way switch.
_
_    Case-switching on attributes of arguments
_
_ Unfortunately, overt switch code relies on Idiom 1, a
_ glaring deviation from the mainstream functional style
_ used to define `iszero' and `succ'. A remedy is at hand:
_ higher-level switch-defining macros that hide Idiom 1.
_
_ Switching is based on one or more "decision attributes",
_ atomic values derived from the arguments. The type of
_ a switch depends on the numbers of decision attributes
_ and parameters.
_
_      switch           decision    parameters
_       type           attributes
_     switch1of1           1            1
_     switch1of2           1            2
_     switch2of2           2            2
_
_ Other switch types are named similarly.
_
_ Warning. The discussion from here through the definition
_ of `switch1of1' may be heavy going. You may wish to skim
_ it then skip around to bring the whole into focus.
_
_ Each switch declaration defines a function and the
_ decision attributes expressed as if they were in
_ the body of the macro to be declared. A switch
_ declaration must be accompanied by `case' macros
_ for all valid sets of values of the decision
_ attributes. (`case', is merely a distinctive
_ synonym for `define'.)
_
define(`case', `define(`$1', `$2')')_
_
_ The name of a case macro comprises the name of the
_ original function and values of the decision
_ attributes separated by underscores. For example,
_ to define `iszero' in switch style, the decision
_ attribute is the head of the argument; and we must
_ define cases, `iszero_', `iszero_0' and `iszero_1'
_ for the three possible values of the attribute
_
_    switch1of1(`iszero',`head($1)')_
_    case(`iszero_',  `T')_
_    case(`iszero_0', `iszero(tail($1))')_
_    case(`iszero_1', `F')_
_
_ In the definition of the switch-declaring macro,
_ `switch1of1', $1 is the name of the function to be
_ declared, notionally like this
_
_    define(`switch1of1',_
_    __`define(`$1', `<casename>(`$1',<attr>)(<arg>)')')
_
_ where <casename> pastes together the function name and a
_ value of the decision attribute, <attr>. The constructed name
_ will be called with argument <arg>. When $1 is `iszero', say,
_ <attr> is `head($1)' and <arg> is $1. Unfortunately $1
_ already means `iszero'. To resolve the clash, we arrange
_ for the appearance of $1 in <arg> to be inserted only when
_ the inner `define' is executed. Thus such instances of $1
_ do not occur in the replacement text of `switch1of1'.
_
_ The trick (Idiom 5) is to replace $<n> by a "dollar macro",
_ `DOL(<n>)', which expands to `$<n>'.
_
define(`DOL',``$$1'')_
_
_ The doubled quotes will be explained later. For `DOL' to be
_ expanded when the inner `define' is executed, the two
_ constructs must appear at the same quotation level. This
_ typically entails flanking `DOL' with outward-facing quotes
_ as in the definition of `switch1of1' below.
_
_ As the number of decision attributes differs among switch
_ types, <casename> needs multiple realizations. We call the
_ macro that expands to a <casename> for one decision
_ attribute `_switch1'.
_
define(`switch1of1',_
__`define(`$1', `_switch1(`$1',$2)('DOL(1)`)')')_
define(`_switch1', `$1_$2')_
_
_ Here is the successor function programmed in switch style.
_
switch1of1(`succ', `head($1)')_
case(`succ_',  one)_
case(`succ_0', `(1,tail($1))')_
case(`succ_1', `(0,succ(tail($1)))')_
_
_ Examples
_
_    switch1of1(`succ', `head($1)')
_    ==> define(`succ', `_switch1(`succ',head($1))($1)')
_
_    succ((1,()))
_    =>  _switch1(`succ', head((1,()))((1,())))
_    =>  _switch1(`succ', 1)((1,())))
_    =>  succ_1((1,()))
_    =>  (0,succ(tail(1,())))
_    ==> (0,succ(()))
_    ==> (0,(1,()))
_
_ Different numbers of switching attributes and parameters
_ are needed for defining other functions. The following
_ definitions are routine variations on `switch1of1'.
_
define(`switch2of2',_
__`define(`$1',_
____`_switch2(`$1',$2,$3)('DOL(1)`,'DOL(2)`)')')_
define(`_switch2', `$1_$2_$3')_
_
define(`switch1of2',_
__`define(`$1',_
____`_switch1(`$1',$2)('DOL(1)`,'DOL(2)`)')')_
_
define(`switch1of3',_
__`define(`$1',_
____`_switch1(`$1',$2)('DOL(1)`,'DOL(2)`,'DOL(3)`)')')_
_
define(`switch2of3',_
__`define(`$1',_
____`_switch2(`$1',$2,$3)('DOL(1)`,'DOL(2)`,'DOL(3)`)')')_
_
_ Here we define some basic arithmetic functions. For
_ illustrative purposes, addition is defined by switching
_ on both arguments, while multiplication is defined by
_ switching on only the first argument. Both definitions
_ can produce insignificant zero bits when inputs have
_ insignificant zero bits.
_
switch2of2(`add', `head($1)',`head($2)')_
case(`add__',   `()')_
case(`add__0',  `$2')_
case(`add__1',  `$2')_
case(`add_0_',  `$1')_
case(`add_0_0', `(0,add(tail($1),tail($2)))')_
case(`add_0_1', `(1,add(tail($1),tail($2)))')_
case(`add_1_',  `$1')_
case(`add_1_0', `(1,add(tail($1),tail($2)))')_
case(`add_1_1', `(0,succ(add(tail($1),tail($2))))')_
_
define(`mul', `if(iszero($2), `()', `_mul($1,$2)')')_
switch1of2(`_mul', `head($1)')_
case(`_mul_',  `()')_
case(`_mul_0', `(0,_mul(tail($1),$2))')_
case(`_mul_1', `add($2,_mul_0($1,$2))')_
_
_ Subtraction. Because of the lack of negative numbers,
_ we provide a "diminish" function, `dim', that clamps
_ negative results at zero.
_
_    dim(a,b) = max(a-b,0)
_
_ The definition of `_dim' exploits the fact that $1>$2
_ when it is called from `dim'. Thus at every recursive
_ level $1>=$2. The last line of `_dim', where $1=(0,...)
_ and $2=(1,...), implements traditonal "borrowing" from
_ the tail of $1 by incrementing the tail of $2.
_
define(`dim', `if(le($1,$2), `()', `trim(_dim($1,$2))')')_
define(`_dim',_
__`if(iszero($2), `$1',_
__`if(eq(head($1),head($2)), `(0,_dim(tail($1),tail($2)))',_
__`if(eq(head($1),1), `(1,_dim(tail($1),tail($2)))'_
__,`(1,_dim(tail($1),succ(tail($2))))')')')')_
_
_ `if' can be defined via `switch1of3', however the resulting
_ implementation is less efficient than the original.
_
_    switch1of3(`if',`$1')_
_    case(`if_T', `$2')_
_    case(`if_F', `$3')_
_
_ The newer definition depends crucially on the doubled
_ quotes in the definition of `DOL' to protect the
_ arguments of a generated call of a case macro from
_ being scanned for macro calls, which would result
_ in unintended executions.
_
_ `rev' reverses the order of elements in a list. The
_ result is accumulated in the second argument of `_rev'.
_
define(`rev', `_rev($1,())')_
define(`_rev',_
__`if(isempty($1), $2,_
__`_rev(tail($1),(head($1),$2))')')_
_
_ `trim' deletes insignifcant zero bits from a number.
_
define(`trim',_
__`if(eq(head($1),`'), `()',_
__`_trim(head($1),trim(tail($1)))')')_
define(`_trim',_
__`if(and(isempty($2),eq($1,0)), `()',_
__`($1,$2)')')_
_
_ Example
_
_    trim(rev((0,(0,(1,())))))
_    ==> trim(_rev((0,(0,(1,()))),()))
_    ==> trim(_rev((0,(1,())),(0,())))
_    ==> trim((1,(0,(0,()))))
_    =>  _trim(1,(0,(0,())))
_    ==> (1,())
_
_ For human consumption, `base2' converts results from
_ lugubrious list form to ordinary binary notation.
_
define(`base2',_
__`if(eq(head($1),`'), `0', `_base2($1)')')_
define(`_base2',_
__`if(eq(head($1),`'), `', `_base2(tail($1))head($1)')')_
_
_ A counter value kept in a macro (per Idiom 4) may be
_ updated by `incr' or read out by expanding it.
_
define(`incr', `define(`$1',succ($1))')_
_
_ Examples
_
_    base2((0,(1,(0,())))) ==> 010
_    base2(trim((0,(1,(0,())))))
_    ==> base2((0,(1,()))) ==> 10
_
_    define(mycounter,zero)
_    incr(`mycounter')
_    incr(`mycounter')
_    base2(mycounter) ==> 10
_
_ `nth(<n>,<a>)' yields item number <n> (counted from 0) in list
_ <a>, or `' if <n> is too big.
_
define(`nth',_
__`if(isempty($2), `',_
__`if(iszero($1),  `head($2)',_
__`nth(dim($1,one),tail($2))')')')_
_
_ Some functional programming staples are `map', `fold' and
_ `zip'. `map' applies a given function to every element of a
_ list. Its use with m4 is hampered by the restriction of list
_ elements to atoms.
_
_ map(<f>, (<x0>,(<x1>,...(<xk>,())))) =
_          (<f>(<x0>),(<f>(<x1>),...(<f>(<xk>),())...))
_
define(`map',_
__`if(isempty($2), `()',_
__`($1(head($2)),map(`$1',tail($2)))')')_
_
_ Folds combine all the elements of a list into a single value by
_ applying a binary operation <op> iteratively, beginning with <z>,
_ and proceeding in right-associative order for `foldr' and left-
_ associative for `foldl'.
_
_ foldr(<op>, <z>, (<x0>,(<x1>,...(<xk>,())))) =
_                  <x0><op>(<x1><op>...(<xk><op><z>)...)
_ <op> is a function of two variables, written here in infix form
_
define(`foldr',_
__`if(isempty($3), $2,_
__`$1(head($3),foldr(`$1',`$2',tail($3)))')')_
_
_ foldl(<op>, <z>, (<x0>,(<x1>,...(<xk>,())))) =
_                  (...((<z><op><x0>)<op><x1>)...)<op><xk>
_
define(`foldl',_
__`if(isempty($3), $2,_
__`foldl(`$1',$1($2,head($3)),tail($3))')')_
_
_ `trim', `rev' and the `base2' workhorse function `_base2'
_ can be defined as folds. `_trim' is as before.
_
define(`trim', `foldr(`_trim',`()',`$1')')_
_
define(`rev', `foldl(`_rev',`()',`$1')')_
define(`_rev',`($2,$1)')_
_
define(`_base2', `foldr(`_base2_',`',`$1')')_
define(`_base2_',`$2$1')_
_
_ Length of a list defined as a fold. The fold calls `succ'
_ with two arguments; `succ' uses only the first one.
_
define(`length',  `foldl(`succ',zero,$1)')_
_
_ zip(<f>, <xs>, <ys>) makes a list of <f>(<x>,<y>) where <x>,<y>
_ are corresponding elements of lists <xs> and <ys>
_
define(`zip',_
__`if(isempty($2), `()',_
__`if(isempty($3), `()',_
__`($1(head($2),head($3)),zip(`$1',tail($2),tail($3)))')')')_
_
_        Iteration
_
_ We define an iterative "for statement" that repeatedly calls
_ a macro with an indexed argument. Its meaning may be described
_ informally thus:
_
_    for(lo,hi,`func') = for i from lo to hi do func(i)
_
define(`for',_
__`if(gt($1,$2), `',_
__`$3($1)'_
__`for(succ($1),$2,`$3')')')_
_
_ `forA' allows a further argument to be passed to `func'.
_ A hack for passing multiple arguments is to hide the
_ separating commas in quotes.
_
_    forA(lo,hi,`func',arg) = for i from low to high do func(arg,i)
_
define(`forA',_
__`if(gt($1,$2), `',_
__`$3($4,$1)'_
__`forA(succ($1),$2,`$3',`$4')')')_
_
_ The double loop below prints a binary addition table.
_
_    define(`table',  `for(zero,one,`row')')
_    define(`row',    `forA(zero,one,`cell',$1)newline()')
_    define(`cell',   `base2(add($1,$2))tab()')
_    define(`tab',    `	')
_    define(`newline',`
_    ')_
_
_ Example
_
_    table() ==>
_    0	1	
_    1	10	
_
_
_        Comparison functions
_
_ Numerical comparison operators may be built on `compare',
_ a function that yields `LT', `EQ', or `GT' according as
_ its first argument is less than, equal to, or greater
_ than the second. The auxiliary function `_compare' breaks
_ ties between tails.
_
switch2of2(`compare', `head($1)',`head($2)')_
case(`compare__',   `EQ')_
case(`compare__0',  `if(iszero($2), `EQ', `LT')')_
case(`compare__1',  `LT')_
case(`compare_0_',  `if(iszero($1), `EQ', `GT')')_
case(`compare_0_0', `compare(tail($1),tail($2))')_
case(`compare_0_1', `_compare(compare(tail($1),tail($2)), LT)')_
case(`compare_1_',  `GT')_
case(`compare_1_0', `_compare(compare(tail($1),tail($2)), GT)')_
case(`compare_1_1', `compare(tail($1),tail($2))')_
_
define(`_compare', `if(eq($1,EQ), `$2', `$1')')_
_
_ The customary two-way comparison predicates can be defined
_ in terms of `compare'. A typical example is the "less than"
_ predicate, `lt'.
_
switch1of2(`lt',`compare($1,$2)')_
case(`lt_LT', `T')_
case(`lt_EQ', `F')_
case(`lt_GT', `F')_
_
_ To avoid the tedium of six such declarations, we
_ capture their pattern in `mkcomp'. `mkcomp' is a
_ third-order function; it defines definers of
_ numerical comparison predicates.
_
define(`mkcomp',_
__`switch1of2(`$1',`compare($'`1,$'`2)')'_
__`case(`$1_LT', `$2')'_
__`case(`$1_EQ', `$3')'_
__`case(`$1_GT', `$4')')_
_
mkcomp(`lt', `T', `F', `F')_
mkcomp(`le', `T', `T', `F')_
mkcomp(`et', `F', `T', `F')_  equal to, differs from eq
mkcomp(`ne', `T', `F', `T')_
mkcomp(`ge', `F', `T', `T')_
mkcomp(`gt', `F', `F', `T')_
_
_ Example
_
_    et((0,()),())  ==> T
_    eql((0,()),()) ==> F
_
_ Comparing atoms for, say, alphabetic order is not a
_ simple task; we need somehow to decide the question
_ consistently for every possible pair of values. The
_ function `cmp' does so for single alphanumerics by
_ finding which member of the pair comes first in a
_ list of the alphabet.
_
define(`cmp',_
__`if(eq(`$1',`$2'), `EQ',_
__`_cmp(`$1',`$2',head(alphabet),tail(alphabet))')')_
_
define(`_cmp',_
__`if(eq(`$1',  `$3'), `LT',_
__`if(eq(`$2',  `$3'), `GT',_
__`if(isempty($4),  `?',_
__`_cmp(`$1',`$2',head(`$4'),tail(`$4'))')')')')_
_
define(`alphabet',_
`(`0',(`1',(`2',(`3',(`4',(`5',(`6',(`7',(`8',(`9',_
(`A',(`a',(`B',(`b',(`C',(`c',(`D',(`d',(`E',(`e',_
(`F',(`f',(`G',(`g',(`H',(`h',(`I',(`i',(`J',(`j',_
(`K',(`k',(`L',(`l',(`M',(`m',(`N',(`n',(`O',(`o',_
(`P',(`p',(`Q',(`q',(`R',(`r',(`S',(`s',(`T',(`t',_
(`U',(`u',(`V',(`v',(`W',(`w',(`X',(`x',(`Y',(`y',_
(`Z',(`z',()))))))))))))))))))))))))))))))))))))))_
))))))))))))))))))))))))')_
_
_ `alphabet' is fragile. The quotes in `alphabet'
_ disappear from the tail copied as an argument of
_ `_cmp'. Then one-letter macro names will affect
_ the alphabet and ruin comparisons as here, where
_ A has effectively been eliminated from the alphabet.
_
_    define(`A',`B')cmp(`z',`A') ==> LT
_
_ Although it is possible with more processing to
_ retain quotes in each copied tail, `cmp' is already
_ costly in both time and space. Iteration over the
_ alphabet involves copying N tails of decreasing
_ length, for a total volume of O(N^2) list elements,
_ where N is the size of the alphabet. Each such
_ iteration creates ghost definitions, up to N^2
_ of which can accumulate over multiple executions
_ of `cmp'.
_
_ Ghost definitions are not only numerous; they are
_ repeatedy redefined. If all alphanumeric pairs are
_ equally likely, an average of about 120 redefinitions
_ and 60 copies of tails will occur for each execution
_ of `cmp'.
_
_ The overheads of redefinition and alphabet-copying can
_ be avoided by using an N^2-way switch instead of linear
_ search in the alphabet. We automate the construction
_ of the  N^2 case declarations with `mkcmp' and a few
_ helper functions defined below.
_
_ Making the case macros all at once will in the long
_ run beat making a lot of ghost definitions for every
_ character comparison. Moreover, the noted fragility
_ of `alphabet' may be mitigated, for no comparison
_ macros refer to `alphabet' after the 3844 case
_ macros for `cmp' have been defined.
_
switch2of2(`cmp', `$1', `$2')_
_
_ The declaration of case macros is automated by`mkcmp',
_ defined below after some helper macros.
_
_ `mkcmp0' makes comparison functions for $1 against
_ $2, which is known to follow $1 in alphabetic order
_
define(`_mkcmp0',_
__`case(`cmp_$1_$2', `LT')'_
__`case(`cmp_$2_$1', `GT')')_
_
_ `_mkcmp1' makes comparison functions for $1 against all
_ characters in list $2.
_
define(`_mkcmp1',_
__`if(isempty($2), `',_
____`_mkcmp0($1,head($2))'_
____`_mkcmp1($1,tail($2))')')_
_
_ `_mkcmp2' makes comparison functions for all pairs
_ constructible from $1 and elements of list $2 of
_ alphabetically following characters.
_
define(`_mkcmp2',_
__`case(`cmp_$1_$1', `EQ')'_
__`if(isempty($2), `',_
____`_mkcmp1($1,$2)'_
____`_mkcmp2(head($2),tail($2))')')_
_
define(`mkcmp', `_mkcmp2(head($1),tail($1))')_
mkcmp(alphabet)_
_
_ Strings can be defined as lists of characters, and
_ compared lexicographically by a list-comparison
_ function, `cmpl'. For the benefit of string
_ manipulation, one-character macro names should be
_ avoided.
_
define(`cmpl',_
__`if(and(isempty($1),isempty($2)), `EQ',_
__`if(isempty($1), `LT',_
__`if(isempty($2), `GT',_
__`_cmpl($1,$2)')')')')_
_
switch1of2(`_cmpl',`cmp(head($1),head($2))')_
case(`_cmpl_LT', `LT')_
case(`_cmpl_EQ', `cmpl(tail($1),tail($2))')_
case(`_cmpl_GT', `GT')_
_
_ Example
_
_    cmpl((A,(N,())), (A,(M,())))
_    ==> _cmpl((A,(N,())), (A,(M,())))
_    ==> switch1(`_cmpl', cmp(A,A))((A,(N,())), (A,(M,())))
_    ==> _cmpl_EQ((A,(N,())), (A,(M,())))
_    ==> cmpl(tail((A,(N,()))), tail((A,(M,()))))
_    ==> GT
_
_ For An N-character alphabet, `mkcmp' creates N^2 macros,
_ and `cmp' takes O(1) time. A set of N macros, `ord_<c>',
_ that give the ordinal position of each character <c>
_ in the alphabet would achieve a different time-space
_ tradeoff, in which `cmp' becomes `compare(ord_$1,ord_$2)'
_ and takes O(log N) time.
_
_
_       Universality
_
_ It's not hard to envision simulating a simple computer. Thus,
_ aside from resource limitations, bare m4 is Turing complete.
_ Here's a workable plan:
_
_ The program counter is maintained by `incr', and converted
_ to string form by `base2'.
_
_ Word addresses are binary numbers.
_
_ The contents of word n are held in a macro named W<n>, where
_ <n> is the base-2 form of the address.
_
_ Words need not be limited in size. Numbers may be kept
_ in the form (sign,magnitude). (Caveat: the sign can't
_ be `+' or `-', because programs can only switch on
_ alphanumerics.)
_
_ Instructions may be of variable length--one word of opcode,
_ and following words that contain immediate operands or
_ addresses expressed in binary form.
_
_ Instruction codes are a limited set of binary numbers <op>,
_ and may be switched on by converting them to base-2 form I<op>.
_
_ An assignment redefines the designated word.
_
_ Other instructions are programmed in styles illustrated above.
_
_ Each instruction updates the location counter appropriately.
_
_ A control macro fetches an opcode according to the location
_ counter.  If the opcode is a halt instruction, the control
_ terminates. Otherwise it calls the designated operation,
_ then calls the control recursively.
_
_ Eric Blake has carried out this general plan for an
_ "intcode" computer and has been able to run impressive
_ demos; see https://adventofcode.com/2019/day/2 and
_ https://repo.or.cz/aoc_eblake.git/commitdiff/
_ 798d9cd09c7b499316713bbe064b420544fbc0f8
_
_
_       Footnotes
_
_ Some efficiency may be gained by replacing separate calls
_ of `head' and `tail' with a single call of a `split' macro,
_ at some loss of clarity. In the version of the successor
_ function below, `_succ' has head and tail arguments, yet
_ no commas are visible in calls for `_succ'.
_
define(`split', `_split$1')_
define(`_split', `$1,$2')_
_
define(`succ', `_succ(split($1))')_
switch1of2(`_succ', `$1')_
case(`_succ_',  one)_
case(`_succ_0', `(1,$2)')_
case(`_succ_1', `(0,_succ(split($2)))')_
_
_
_       Acknowledgement and Colophon
_
_ I am indebted to Eric Blake for his careful reading of, and
_ insightful comments on a previous version of this program.
_ He inspired the current implementation of the critical
_ dollar macro and suggested `split'.
_
_ M. Douglas McIlroy
_ Dartmouth College
_ August 2020
_ January 2021: comments tweaked
_ September 2024: replace eq to work on arbitrary atoms
_ June-July 2025: revise switch declarations; add iteration,
_     comparison functions, map, fold, zip and appendix
_
_       Appendix
_
_ This appendix rounds out the usual complement of
_ arithmetic operators on natural numbers. No new
_ programming techniques are introduced. Like `add' and
_ `mul', these operators yield "normal" numbers with
_ no insignificant zeros when given normal inputs, but
_ are not guaranteed to do so otherwise.
_
_ Square.
_
define(`square', `mul($1,$1)')_
_
_ Exponentiation. pow(a,n) returns a^n. If n=b+2*n', where
_ b is 0 or 1, pow(a,n) = a^b*pow(a^n')^2. As the code is
_ written, 0^0 is taken to be 1.
_
define(`pow',_
__`if(iszero($2), `one',_
__`if(eq(head($2),0), `square(pow($1,tail($2)))',_
__`mul($1,square(pow($1,tail($2))))')')')_
_
_ Division. The customary integer operators `div' and `mod'_
_ are defined in terms of a quotient-remainder operator, `qr'.
_
define(`div',`head(qr($1,$2))')_
define(`mod',`tail(qr($1,$2))')_
_
_ qr(n,d) yields a pair (q,r), the integer quotient and
_ remainder for the division operation n/d, where d>0.
_ The result is expressed in "strict (q-r) form".
_
_ For expository purposes, we assume d fixed and write n
_ in "q-r form", [q,r], where n = q*d+r. When r<d the
_ representation is called "strict".
_
_ If n<d, qr(n,d) yields [0,n]. Otherwise, let the binary
_ representation of n be (b,n'), where b is 0 or 1 and
_ n = b+2*n'. Recursively call qr(n',d) to get [q',r'],
_ the strict q-r form of n'.
_
_ Now n-b = 2*n' = 2*[q',r'] = [2*q',2*r'], whence
_ [2*q',b+2*r'] is a [q-r] representation of n. Since
_ b+2*r' can be as large as 2*d-1, this representation
_ is not strict, but can be made so by the function
_
_   _qrnorm(d,q,r) = {if r<d then [q,r] else [q+1,r-d]}
_
_ `trim' removes insignificant zero bits that `_qr'
_ produces for some normal inputs. (0,q) is a shortcut
_ for mul(two,q).
_
define(`qr',_
__`define(`_qrt', _qr($1,$2))'_
__`(trim(head(_qrt)),trim(tail(_qrt)))')_
define(`_qr',_
__`if(lt($1,$2),  `(zero,$1)',_
__`define(`_qrt', _qr(tail($1),$2))'_
__`_qrnorm($2,`(0,head(_qrt))',_
____`if(eq(head($1),0), `(0,tail(_qrt))',_ b=0
____`(1,tail(_qrt))')')')')_               b=1
define(`_qrnorm',_
__`if(lt($3,$1), `($2,$3)',_
__`(succ($2),dim($3,$1))')')_
_
_
_       Function summary
_
_ if            conditional
_ not, and, or, xor
_               boolean operators
_ eq            test equality of atoms
_ cmp           compare atoms
_ case, switch[12]of[123]
_               switch definers
_
_       Numerics
_
_ succ          successor
_ incr          increment a named number
_ iszero        test for zero
_ add           addition
_ mul           multiplication
_ dim           "diminish",  subtraction clamped at zero
_ qr            quotient and remainder
_ div           integer quotient
_ mod           integer remainder
_ pow           integer power
_ square
_ compare       comparison yielding LT, EQ, GT
_ lt, le, et, ne, ge, gt
_               numeric predicates (et = equal to)
_ trim          delete insignificant zeros
_ for           iterate a function over numerical range
_ forA          ditto with extra arguments
_ zero, one, two, three, four
_               simple constants
_
_       List handling
_
_ head, tail    decomposition
_ isempty       test emptiness
_ eql           test equality of atom lists
_ cmpl          compare lists of atoms lexicographically
_ nth           get nth element
_ map           apply function elementwise
_ foldr         reduce to scalar by right-associated operator
_ foldl         reduce to scalar by left-assocciated operator
_ rev           reverse
_ length        count elements
_ zip           apply function to combine lists elementwise
