# Keys
- Emit clean c code
- default is value, if not stated otherwise
- same syntax means exactly the same thing, not type-based

# File Structure
- `joc.h` C backend
- `joc.ts` Compiler in typescript (prototype stage)
- `Makefile` make it

# State

# How To Use
- compile with `joc.ts` with `tsc joc.ts`, or use any TS to JS converter
- run JS, in node, or put in browser `<script src="joc.js"></script>` or whatever ***(its direct vanilla JS, no modules fights!)***
- call this function `test()` and see what it out in console