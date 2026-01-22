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

interface Tokenizer {
    src: string,
    pos: int, line: int, col: int,
    mpos: int, mcol: int, mline: int,
}

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
    return { src: text, pos: 0, line: 0, col: 0, mcol: 0, mpos: 0, mline: 0 };
}

function token(tok: Tokenizer): Token | null {
    let val = '';
    let prev: TokenType = 0;

    tok.mpos = tok.pos;
    tok.mcol = tok.col;
    tok.mline = tok.line;

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
            return { type: prev, val, pos: tok.mpos, line: tok.mline, col: tok.mcol };
        prev = code;
    }

    if (prev)
        return { type: prev, val, pos: tok.mpos, line: tok.mline, col: tok.mcol };

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

