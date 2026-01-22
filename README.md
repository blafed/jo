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