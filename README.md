# Status
**Under development**

# Keys
- Emit clean c code
- default is value, if not stated otherwise
- same syntax means exactly the same thing, not type-based

# Files
- `joc.h` C backend
- `joc.ts` compiler in typescript (prototype stage)
- `ex/` examples

# Roadmap
- [X] tokenizer (dump tokens)
- [X] synizer (dump syntax tree)
- [ ] semantics (meaningful syntax tree)
- [ ] simulation (validation, ...)
- [ ] emit (clean c code)

# How
- compile `joc.ts` to JS
- run JS directly *(in browser, nodejs, ...)* **[no module nonsense!]**
- call `test()` and see what it prints

# Syntax

*NOT completed* !

---

### Functions

```jo
foo(){}        // define
foo() int {}  // define with type
foo()          // call
```



### Fields / Variables

```jo
foo := 1        // immutable field
foo` := 1       // mutable field
foo MyType      // declared with type
```

```jo
bar := 1        // OK
bar := int{1}   // OK
bar int         // OK
bar int = 1     // OK
bar int := 1    // NOT OK
```



### Structs

```jo
Data [x int, y int]
Cat  [name str, age int, friends Cat[]]
```



### Arrays & Buns

```jo
x int[]    // bun (fixed, heap, count unknown at comptime)
y int[4]   // array (fixed, inlined/stack, count known at comptime)
z := []{1,2,3}
```



### References

```jo
x := 0
y := &x

x == y     // true, deep value equality
x &== y    // true, shallow ref equality
```



### Copy

```jo
Data [x int, y int, childs Data[]]

y := Data{
    x 0,
    y 100,
    childs []{
        {x 50, y 50},
        {x 40, y 40}
    }
}

x := y     // deep copy
x == y     // true
```



### Nullables

```jo
x := null

if x: log('not null')
else log('null')
```



### Equality with References

```jo
Data [val int, ref Data&]

x := Data{1, &something}
y := Data{1, &something}

x == y    // x.val == y.val && x.ref &== y.ref
```



### Enums

```jo
Day enum {foo, bar, baz}            // foo=0, bar=1, baz=2
Condition enum:flag {foo, bar, baz} // foo=1, bar=2, baz=4
```



### Aliases

```jo
number alias int
x number = 0
```



### Keywords

```jo
if, else, for, while, when
return, break, continue
enum, alias
typeof, sizeof, nameof
assert
```

### Reserved Namings

#### ALWAYS 
```jo
// Only for jo_xxx C API

jo_xxx
JO_xxx
jO_xxx
Jo_xxx
```

#### DEPENDS
```jo
//given 'xxx' is a struct name:

xxx [...] 

// these are reserved names:
xxx_new
xxx_del
xxx_cpy
xxx_equ

// auto generated math operations for pure math types (only if xxx is a pure math type):

xxx alias int[]
xxx [a int, b int]
xxx [int,int,int]
xxx alias int[4]

xxx_sum
xxx_sub
xxx_mul
xxx_div
xxx_neg
xxx_sum_scalar
xxx_sub_scalar
xxx_mul_scalar
xxx_div_scalar
xxx_neg_scalar

//so when writing:
a xxx, b xxx, c xxx
c = a + b

//compiler maps it to:
c = xxx_sum(a, b);

```



### Symbol Meanings

```jo
???     // not implemented
:=      // declare and assign
==      // value equality (deep)
&==     // ref equality (shallow)

*x      // new
&x      // ref
^x      // move
^[]foo  // expand
[]{}    // array
[*]{}   // bun
```

---