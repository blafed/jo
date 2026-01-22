# Status
**Under development**

# Keys
- Emit clean c code
- default is value, if not stated otherwise
- same syntax means exactly the same thing, not type-based

# Files
- `joc.h` C backend
- `joc.ts` Compiler in typescript (prototype stage)
- `Makefile` make it

# Roadmap
- [X] tokenizer (dump tokens)
- [X] synizer (dump syntax tree)
- [ ] semantics (meaningful syntax tree)
- [ ] simulation (validation, ...)
- [ ] emit (clean c code)

# How
- compile with `joc.ts` with `tsc joc.ts`, or use any TS to JS converter
- run JS, in node, or put in browser `<script src="joc.js"></script>` or whatever ***(its direct vanilla JS, no modules fights!)***
- call this function `test()` and see what it out in console