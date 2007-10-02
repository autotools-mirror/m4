divert(`-1')
# foreachq(x, `item_1, item_2, ..., item_n', stmt)
#   quoted list, alternate improved version
define(`foreachq',
`pushdef(`$1')_$0(`$1', `$3'ifelse(`$2', `', `',
  `, $2'))popdef(`$1')')
define(`_foreachq', `ifelse(`$#', `2', `',
  `define(`$1', `$3')$2`'$0(`$1', `$2'ifelse(`$#', `3', `',
    `, shift(shift(shift($@)))'))')')
divert`'dnl
