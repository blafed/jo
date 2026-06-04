# Keys
- emit clean C code
- default is value, if not stated otherwise
- manual memory management
- platform hook to bootstrap
- code using data


# Roadmap
- [X] [`0.1`](ex/0.1.jo): data language 
- [ ] [`0.2`](ex/0.2.jo): adding booleans, nil and float E notation + C api
- [ ] [`0.3`](ex/0.3.jo): eval const expressions + let the parser manage its own memory


# 0.1 syntax

```jo
age 10
pi 3.14
msg 'hi there'
point {x -1, y 1}
cards {1, 2, 3, 'jack', 'ace'}
inventory {
    {type 'sword', count 2}
    {type 'shield', count 1}
}
```

```jo
//comment
/*
comment block
*/
```