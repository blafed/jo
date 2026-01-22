type int = number;


//==================
// #region TOKENS
//==================

const enum TokenType {
    none,
    id, //sequence of alphabet (multople)
    num,  //sequence of digits (multiple)
    sym, //any symbol single only (single)
    sep, //sequence of whitespace of same type, flattened (single) 
}

interface Tokenizer { src: string, pos: int, line: int, col: int }

function is_digit(c: string) { return c >= '0' && c <= '9'; }
function is_alpha(c: string) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
function is_alphanum(c: string) { return is_digit(c) || is_alpha(c); }
function is_whitespace(c: string) { return c == ' ' || c == '\t' || c == '\n'; }
function is_newline(c: string) { return c == '\n'; }
function is_control(c: string) { return (c < ' ' || c == '\x7F') && !is_whitespace(c); }
function is_sep(c: string) { return c == '\n' || c == ','; }


function char_code(c: string): TokenType {
    if (is_whitespace(c)) return TokenType.sep;
    else if (is_alpha(c)) return TokenType.id;
    else if (is_digit(c)) return TokenType.num;
    else return TokenType.sym;
}


interface Token { type: TokenType, val: string, pos: int, line: int, col: int }

function _cursor_inc(t: Tokenizer) {
    t.pos++;
    t.col++;
    if (is_newline(t.src[t.pos - 1])) {
        t.line++;
        t.col = 0;
    }
}

function tokenizer(text: string): Tokenizer {
    return { src: text, pos: 0, line: 0, col: 0 };
}

function token(tok: Tokenizer): Token | null {
    let val = '';
    let prev: TokenType = 0;

    let mpos = tok.pos;
    let mcol = tok.col;
    let mline = tok.line;

    const enum Kind {
        none, advance, emit, emit_advance,
    }

    while (tok.pos < tok.src.length) {
        let c = tok.src[tok.pos];
        let code = char_code(c);

        let kind: Kind = 0;
        switch (code) {
            case TokenType.sym:
                if (val) kind = Kind.emit;
                else {
                    kind = Kind.emit_advance;
                    prev = code;
                }
                break;
            case TokenType.id:
            case TokenType.num:
                if (code != prev && prev) kind = Kind.emit;
                else kind = Kind.advance;
                break;
            case TokenType.sep:
                if (val.length && val[val.length - 1] != c) kind = Kind.emit;
                else kind = Kind.advance;
                break;
            default: throw 'unsupported char ' + c;
        }

        if (kind & Kind.advance) {
            val += c;
            _cursor_inc(tok);
        }

        if (kind & Kind.emit)
            return { type: prev, val, pos: mpos, line: mline, col: mcol };
        prev = code;
    }

    if (prev)
        return { type: prev, val, pos: mpos, line: mline, col: mcol };

    return null;
}


function tokenize(str: string) {
    let tok = tokenizer(str);
    let list: Token[] = [];
    let t;
    while (t = token(tok)) {
        list.push(t);
    }
    return list;
}



//==================
// #region SYNTAX
//==================
//

const enum SynType {
    sep = -1, //temporary
    atom, //name | number | sym
    seq, //things split by space
    group, //things split by comma/newline
    slug, //qouted things
    string, //a string block
    comment, //a comment block
}

interface Syn {
    parent: Syn,
    type: SynType,
    anot?: string, //sub type annotation (i = number, n = name, s = symbol, etc...)
    child?: Syn[],
    val?: string
}

function synize(tokens: Token[]): Syn {
    let current: Syn = { anot: "root", type: SynType.group, child: [], parent: null! };

    let anot = '';
    let val = '';
    let escape = '';
    let continus = '';

    let pos = 0;

    const match = (str: string, pos: int) => {
        if (str.length + pos >= tokens.length) return false;
        let buf = '';
        for (let i = 0; i < str.length; i++) {
            buf += tokens[pos + i].val;
            if (buf == str) return true;
        }
        return false;
    };

    const sep = () => {
        flush();
        if (current.child!.length && current.child![current.child!.length - 1].type == SynType.sep) return;
        let s: Syn = { type: SynType.sep, parent: current };
        current.child!.push(s);
    };

    const flush = () => {
        if (!val)
            return;

        if (escape == '\\') {
            let res = decode_escape(val, 0);
            if (res.ok)
                val = res.value + val.slice(res.consumed);
        }

        let s: Syn = { type: current.type >= SynType.string && escape != '$' ? SynType.slug : SynType.atom, parent: current };
        if (val == '.')
            s.anot = 's';

        current.child!.push(s);

        if (anot) s.anot = anot;
        if (val) s.val = val;

        anot = '';
        val = '';
        escape = '';
        continus = '';
    };

    const finish = () => {
        if (current.type != SynType.group) return;
        const out: Syn[] = [];
        let buf: Syn[] = [];

        const flush = () => {
            if (buf.length === 1) out.push(buf[0]);
            else if (buf.length > 1) out.push({ type: SynType.seq, parent: current, child: buf });
            buf = [];
        };

        for (const ch of current.child!) {
            if (ch.type === SynType.sep) flush();
            else buf.push(ch);
        }

        flush();
        current.child = out;
    };


    const pop = () => {
        if (!current.parent) throw 'wtf';

        flush();
        finish();

        if (current.parent)
            current = current.parent;
    };

    const push = (type: SynType, anot?: string) => {
        flush();
        let s: Syn = { type, anot, parent: current, child: [] };
        current.child!.push(s);
        current = s;
    };

    const solve_atom = (t: Token) => {

        if (t.type == TokenType.id || t.type == TokenType.num || t.type == TokenType.sym && t.val == '_') {
            if (!anot || anot == 's' && t.val == '.') anot = t.type == TokenType.id ? 'i' : 'n';
            else if (anot == 's') { flush(); pos--; }
            if (anot == 'i' || anot == 'n') val += t.val;
        } else if (t.type == TokenType.sym) {
            if (t.val == '.' && (!anot || anot != 's') && (!val.length || val[val.length - 1] != '.')) {
                continus = '.';
                val += t.val;
            }
            else {
                if (anot != 's') flush();
                anot = 's';
                val += t.val;
            }
        }
    }

    const noraml = function (t: Token) {
        if (current.type == SynType.string) {
            if (escape) {
                solve_atom(t);
            } else
                val += t.val;
        }
        else if (current.type == SynType.comment) val += t.val;
        else solve_atom(t);
    };

    const advance = function (i: number) {
        if (i < 1) throw 'wtf';
        pos += i - 1;
    }


    while (pos < tokens.length) {
        let t = tokens[pos];


        switch (t.type) {
            case TokenType.id:
            case TokenType.num:
                noraml(t);
                break;
            case TokenType.sym:
                switch (t.val) {
                    case ',':
                        if (current.type <= SynType.group) sep();
                        else noraml(t);
                        break;
                    case '*':
                        if (current.type == SynType.comment && match('*/', pos)) {
                            pop();
                            advance(2);
                        } else noraml(t);
                        break;
                    case '/':
                        if (current.type <= SynType.group) {
                            if (match('//', pos)) {
                                push(SynType.comment, '//');
                                advance(2);
                            } else if (match('/*', pos)) {
                                push(SynType.comment, '/*');
                                advance(2);
                            } else noraml(t);
                        }
                        break;
                    case '\\':
                        if (current.type == SynType.string) {
                            if (!escape) {
                                flush();
                                escape = '\\';
                            }
                            else noraml(t);
                        } else noraml(t);
                        break;
                    case '$':
                        if (current.type == SynType.string && current.anot![0] == "'") {
                            if (match('${', pos) && !escape) {
                                push(SynType.group);
                                advance(2);
                            } else {
                                if (!escape) {
                                    flush();
                                    escape = '$';
                                } else noraml(t);
                            }
                        }
                        else noraml(t);

                        break;
                    case "'": case '"':
                        let triple = t.val == '"' && match('"""', pos) || t.val == "'" && match("'''", pos);
                        if (triple) {
                            if (current.type == SynType.string && current.anot![0] == t.val && current.anot!.length == 3) {
                                pop();
                                advance(3);
                            }
                            else if (current.type <= SynType.group) {
                                push(SynType.string, t.val == '"' ? '"""' : "'''");
                                advance(3);
                            } else triple = false;
                        }

                        if (!triple) {
                            if (current.type == SynType.string && current.anot![0] == t.val && escape != '\\') pop();
                            else if (current.type <= SynType.group) push(SynType.string, t.val);
                            else noraml(t);
                        }
                        break;
                    case '(': case '[': case '{':
                        if (current.type <= SynType.group) push(SynType.group, t.val);
                        else noraml(t);
                        break;
                    case ')': case ']': case '}':
                        if (current.type <= SynType.group) pop();
                        else noraml(t);
                        break;
                    default: noraml(t); break;

                }
                break;
            case TokenType.sep:
                if (t.val[0] == ' ' || t.val[0] == '\t') {
                    if (current.type == SynType.string) {
                        if (escape) flush();
                        else val += t.val;
                    } else if (current.type == SynType.comment) val += t.val;
                    else if (val && !continus) flush();
                    // if (current.type == SynType.string && escape != '$' || current.type == SynType.comment) val += t.val;
                    // else if (val && !continus) flush();
                } else if (t.val[0] == '\n') {
                    if (current.type <= SynType.group) { sep(); }
                    else if (current.type == SynType.comment && current.anot == '//') pop();
                    else noraml(t);
                }
                break;
        }

        pos++;
    }

    flush();
    while (current.parent) pop();
    finish();

    return current;
}

function print_syn(s: Syn, d = 0): string {
    const pad = d + '  '.repeat(d);

    // atom
    if (s.type === SynType.atom) {
        let out = `${pad}<${s.anot}>• ${s.val}`;
        return out;
    }

    if (s.type == SynType.slug) {
        let out = `${pad}: ${s.val}`;
        return out;
    }

    // const-enum-safe type names
    const name =
        s.type === SynType.seq ? 'SEQ' :
            s.type === SynType.group ? 'group' :
                s.type === SynType.sep ? 'SEP' :
                    s.type === SynType.string ? 'string' :
                        s.type === SynType.comment ? 'comment' :
                            '???';

    let out = `${pad}▸ ${name}`;
    if (s.anot) out += ` ${s.anot}`;

    if (s.child) {
        for (const c of s.child) {
            out += '\n' + print_syn(c, d + 1);
        }
    }

    return out;
}


//===================
// #region UTILS
//===================

type EscapeResult = { ok: true; value: string; consumed: number } | { ok: false; error: string };
function decode_escape(src: string, i: number): EscapeResult {
    if (i >= src.length)
        return { ok: false, error: "unterminated escape" };

    const c = src[i];

    // fast path: single-char escapes
    switch (c) {
        case 'n': return { ok: true, value: '\n', consumed: 1 };
        case 't': return { ok: true, value: '\t', consumed: 1 };
        case 'r': return { ok: true, value: '\r', consumed: 1 };
        case '0': return { ok: true, value: '\0', consumed: 1 };
        case '"': return { ok: true, value: '"', consumed: 1 };
        case "'": return { ok: true, value: "'", consumed: 1 };
        case '\\': return { ok: true, value: '\\', consumed: 1 };
    }

    // hex: \xNN
    if (c === 'x') {
        if (i + 2 >= src.length)
            return { ok: false, error: "short \\x escape" };

        const hex = src.slice(i + 1, i + 3);
        if (!/^[0-9a-fA-F]{2}$/.test(hex))
            return { ok: false, error: "invalid hex escape" };

        return {
            ok: true,
            value: String.fromCharCode(parseInt(hex, 16)),
            consumed: 3
        };
    }

    // unicode: \uNNNN
    if (c === 'u') {
        if (i + 4 >= src.length)
            return { ok: false, error: "short \\u escape" };

        const hex = src.slice(i + 1, i + 5);
        if (!/^[0-9a-fA-F]{4}$/.test(hex))
            return { ok: false, error: "invalid unicode escape" };

        return {
            ok: true,
            value: String.fromCharCode(parseInt(hex, 16)),
            consumed: 5
        };
    }

    return { ok: false, error: "unknown escape" };
}


//===================
// #region MISC
//===================

function test_code(code: string) {
    let tokens = tokenize(code);
    let syn = synize(tokens);

    console.log('TOKENS:');
    console.log(tokens);
    console.log('SYNTAX:');
    console.log(print_syn(syn));
}

function test() {
    test_code(`
        foo() {
            log('hi from jo')
        }
    `);
}