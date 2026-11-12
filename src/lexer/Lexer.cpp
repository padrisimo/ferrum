#include "lexer/Lexer.h"

#include <utility>

namespace ferrum {

LexError::LexError(std::string message, std::uint32_t atLine, std::uint32_t atColumn)
    : std::runtime_error(std::to_string(atLine) + ":" + std::to_string(atColumn) + ": " + message),
      message_(std::move(message)),
      line_(atLine),
      column_(atColumn) {}

namespace {

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isIdentStart(char c) {
    return isAlpha(c) || c == '_';
}

bool isIdentChar(char c) {
    return isIdentStart(c) || isDigit(c);
}

bool isHexDigit(char c) {
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int hexValue(char c) {
    if (isDigit(c)) {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return c - 'A' + 10;
}

bool isDigitOfBase(char c, int base) {
    switch (base) {
    case 2:
        return c == '0' || c == '1';
    case 8:
        return c >= '0' && c <= '7';
    case 16:
        return isHexDigit(c);
    default:
        return isDigit(c);
    }
}

/// "'x'" for printable ASCII, "0x1F" for everything else.
std::string describeByte(char c) {
    const auto byte = static_cast<unsigned char>(c);
    if (byte >= 0x20 && byte < 0x7F) {
        return std::string("'") + c + "'";
    }
    static const char digits[] = "0123456789ABCDEF";
    std::string out = "0x";
    out += digits[byte >> 4];
    out += digits[byte & 0x0F];
    return out;
}

[[noreturn]] void fail(std::string message, std::uint32_t line, std::uint32_t column) {
    throw LexError(std::move(message), line, column);
}

/// Length in bytes of the well-formed UTF-8 sequence starting at s[i], or 0 if
/// the bytes there are not well-formed (bad lead or continuation byte,
/// truncated, overlong, surrogate, or beyond U+10FFFF).
std::size_t utf8SequenceLength(std::string_view s, std::size_t i) {
    auto byteAt = [&](std::size_t k) { return static_cast<unsigned>(static_cast<unsigned char>(s[k])); };
    const unsigned lead = byteAt(i);
    if (lead < 0x80) {
        return 1;
    }
    std::size_t length = 0;
    unsigned codePoint = 0;
    unsigned minimum = 0;
    if (lead >= 0xC2 && lead <= 0xDF) {
        length = 2;
        codePoint = lead & 0x1F;
        minimum = 0x80;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        length = 3;
        codePoint = lead & 0x0F;
        minimum = 0x800;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        length = 4;
        codePoint = lead & 0x07;
        minimum = 0x10000;
    } else {
        return 0;
    }
    if (i + length > s.size()) {
        return 0;
    }
    for (std::size_t k = 1; k < length; ++k) {
        const unsigned continuation = byteAt(i + k);
        if ((continuation & 0xC0) != 0x80) {
            return 0;
        }
        codePoint = (codePoint << 6) | (continuation & 0x3F);
    }
    if (codePoint < minimum || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
        return 0;
    }
    return length;
}

}  // namespace

Lexer::Lexer(std::string_view source) : src_(source) {
    // Skip a UTF-8 byte order mark.
    if (src_.size() >= 3 && static_cast<unsigned char>(src_[0]) == 0xEF &&
        static_cast<unsigned char>(src_[1]) == 0xBB && static_cast<unsigned char>(src_[2]) == 0xBF) {
        pos_ = 3;
    }
}

bool Lexer::atEnd() const {
    return pos_ >= src_.size();
}

char Lexer::peek(std::size_t offset) const {
    return pos_ + offset < src_.size() ? src_[pos_ + offset] : '\0';
}

char Lexer::advance() {
    const char c = src_[pos_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (!atEnd() && src_[pos_] == expected) {
        advance();
        return true;
    }
    return false;
}

void Lexer::skipTrivia() {
    while (!atEnd()) {
        const char c = src_[pos_];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
            advance();
        } else if (c == '#') {
            // Comment: runs to the end of the line. This covers "##" too; doc
            // comments get their meaning in Phase 4.
            while (!atEnd() && src_[pos_] != '\n') {
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::make(TokKind kind, std::size_t start, std::uint32_t line, std::uint32_t column) const {
    return Token{kind, src_.substr(start, pos_ - start), line, column};
}

Token Lexer::next() {
    skipTrivia();

    const std::size_t start = pos_;
    const std::uint32_t line = line_;
    const std::uint32_t column = column_;

    if (atEnd()) {
        return Token{TokKind::Eof, std::string_view{}, line, column};
    }

    const char c = src_[pos_];
    if (isIdentStart(c)) {
        return lexWord(start, line, column);
    }
    if (isDigit(c)) {
        return lexNumber(start, line, column);
    }
    if (c == '"') {
        return lexString(start, line, column);
    }

    advance();
    switch (c) {
    case '+':
        return make(TokKind::Plus, start, line, column);
    case '-':
        return make(match('>') ? TokKind::Arrow : TokKind::Minus, start, line, column);
    case '*':
        return make(TokKind::Star, start, line, column);
    case '/':
        return make(TokKind::Slash, start, line, column);
    case '%':
        return make(TokKind::Percent, start, line, column);
    case '&':
        return make(match('&') ? TokKind::AmpAmp : TokKind::Amp, start, line, column);
    case '|':
        return make(match('|') ? TokKind::PipePipe : TokKind::Pipe, start, line, column);
    case '^':
        return make(TokKind::Caret, start, line, column);
    case '~':
        return make(TokKind::Tilde, start, line, column);
    case '<':
        if (match('<')) {
            return make(TokKind::Shl, start, line, column);
        }
        return make(match('=') ? TokKind::LtEq : TokKind::Lt, start, line, column);
    case '>':
        if (match('>')) {
            return make(TokKind::Shr, start, line, column);
        }
        return make(match('=') ? TokKind::GtEq : TokKind::Gt, start, line, column);
    case '=':
        return make(match('=') ? TokKind::EqEq : TokKind::Assign, start, line, column);
    case '!':
        return make(match('=') ? TokKind::NotEq : TokKind::Bang, start, line, column);
    case '.':
        if (match('.')) {
            return make(match('=') ? TokKind::DotDotEq : TokKind::DotDot, start, line, column);
        }
        return make(TokKind::Dot, start, line, column);
    case ',':
        return make(TokKind::Comma, start, line, column);
    case ':':
        return make(TokKind::Colon, start, line, column);
    case ';':
        return make(TokKind::Semicolon, start, line, column);
    case '?':
        return make(TokKind::Question, start, line, column);
    case '(':
        return make(TokKind::LParen, start, line, column);
    case ')':
        return make(TokKind::RParen, start, line, column);
    case '{':
        return make(TokKind::LBrace, start, line, column);
    case '}':
        return make(TokKind::RBrace, start, line, column);
    case '[':
        return make(TokKind::LBracket, start, line, column);
    case ']':
        return make(TokKind::RBracket, start, line, column);
    default:
        break;
    }

    if (c == '\'') {
        fail("unexpected character ''' (Ferrum has no character literals; use a double-quoted string)",
             line, column);
    }
    if (static_cast<unsigned char>(c) >= 0x80) {
        fail("unexpected non-ASCII character (identifiers are ASCII-only; non-ASCII text is only "
             "allowed inside strings and comments)",
             line, column);
    }
    fail("unexpected character " + describeByte(c), line, column);
}

Token Lexer::lexWord(std::size_t start, std::uint32_t line, std::uint32_t column) {
    while (isIdentChar(peek())) {
        advance();
    }
    const std::string_view text = src_.substr(start, pos_ - start);
    if (const std::optional<TokKind> kind = lookupWord(text)) {
        return make(*kind, start, line, column);
    }
    return make(TokKind::Ident, start, line, column);
}

Token Lexer::lexNumber(std::size_t start, std::uint32_t line, std::uint32_t column) {
    auto columnAt = [&](std::size_t offset) { return column + static_cast<std::uint32_t>(offset); };
    auto consumeDecimalDigits = [&]() {
        while (isDigit(peek()) || peek() == '_') {
            advance();
        }
    };

    int base = 10;
    bool isFloat = false;

    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'o' || peek(1) == 'b')) {
        base = peek(1) == 'x' ? 16 : (peek(1) == 'o' ? 8 : 2);
        advance();
        advance();
        // Take every identifier character so that a bad digit is reported as
        // such, instead of being left over as a separate token.
        while (isIdentChar(peek())) {
            advance();
        }
        const std::string_view lex = src_.substr(start, pos_ - start);
        const char* baseName = base == 16 ? "hexadecimal" : (base == 8 ? "octal" : "binary");
        bool anyDigit = false;
        for (std::size_t i = 2; i < lex.size(); ++i) {
            if (lex[i] == '_') {
                continue;
            }
            if (!isDigitOfBase(lex[i], base)) {
                fail("invalid digit " + describeByte(lex[i]) + " in " + baseName + " literal", line,
                     columnAt(i));
            }
            anyDigit = true;
        }
        if (!anyDigit) {
            fail(std::string(baseName) + " literal has no digits", line, column);
        }
    } else {
        consumeDecimalDigits();
        if (peek() == '.' && isDigit(peek(1))) {
            advance();
            consumeDecimalDigits();
            isFloat = true;
        }
        if ((peek() == 'e' || peek() == 'E') &&
            (isDigit(peek(1)) || ((peek(1) == '+' || peek(1) == '-') && isDigit(peek(2))))) {
            advance();
            if (peek() == '+' || peek() == '-') {
                advance();
            }
            consumeDecimalDigits();
            isFloat = true;
        }
    }

    // An underscore is only allowed between two digits.
    const std::string_view lex = src_.substr(start, pos_ - start);
    for (std::size_t i = 0; i < lex.size(); ++i) {
        if (lex[i] != '_') {
            continue;
        }
        const bool betweenDigits =
            i > 0 && i + 1 < lex.size() && isDigitOfBase(lex[i - 1], base) && isDigitOfBase(lex[i + 1], base);
        if (!betweenDigits) {
            fail("'_' must be placed between two digits", line, columnAt(i));
        }
    }

    if (isIdentChar(peek())) {
        fail("invalid suffix on numeric literal (Ferrum literals take no suffix; use 'ut' to convert)",
             line_, column_);
    }

    return make(isFloat ? TokKind::FloatLit : TokKind::IntLit, start, line, column);
}

Token Lexer::lexString(std::size_t start, std::uint32_t line, std::uint32_t column) {
    advance();  // opening quote
    for (;;) {
        if (atEnd() || src_[pos_] == '\n' || src_[pos_] == '\r') {
            fail("unterminated string literal", line, column);
        }
        const char c = src_[pos_];
        if (c == '"') {
            advance();
            break;
        }
        if (c == '\0') {
            fail("string literal contains a raw NUL byte (write it as \\0)", line_, column_);
        }
        if (c == '\\') {
            const std::uint32_t escapeLine = line_;
            const std::uint32_t escapeColumn = column_;
            advance();
            if (atEnd() || peek() == '\n' || peek() == '\r') {
                fail("unterminated string literal", line, column);
            }
            const char e = advance();
            switch (e) {
            case 'n':
            case 't':
            case 'r':
            case '0':
            case '\\':
            case '"':
            case '{':
            case '}':
                break;
            case 'x':
                if (!isHexDigit(peek()) || !isHexDigit(peek(1))) {
                    fail("\\x escape needs two hexadecimal digits", escapeLine, escapeColumn);
                }
                advance();
                advance();
                break;
            default:
                fail("invalid escape sequence: '\\' followed by " + describeByte(e), escapeLine,
                     escapeColumn);
            }
            continue;
        }
        if (static_cast<unsigned char>(c) >= 0x80) {
            const std::size_t length = utf8SequenceLength(src_, pos_);
            if (length == 0) {
                fail("invalid UTF-8 in string literal", line_, column_);
            }
            for (std::size_t i = 0; i < length; ++i) {
                advance();
            }
            continue;
        }
        advance();
    }
    return make(TokKind::StrLit, start, line, column);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    for (;;) {
        tokens.push_back(next());
        if (tokens.back().kind == TokKind::Eof) {
            break;
        }
    }
    return tokens;
}

std::string unescapeString(std::string_view stringLexeme) {
    std::string out;
    if (stringLexeme.size() < 2) {
        return out;
    }
    out.reserve(stringLexeme.size());
    const std::size_t end = stringLexeme.size() - 1;  // index of the closing quote
    std::size_t i = 1;
    while (i < end) {
        const char c = stringLexeme[i++];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        const char e = stringLexeme[i++];
        switch (e) {
        case 'n':
            out.push_back('\n');
            break;
        case 't':
            out.push_back('\t');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case '0':
            out.push_back('\0');
            break;
        case 'x': {
            const int high = hexValue(stringLexeme[i]);
            const int low = hexValue(stringLexeme[i + 1]);
            i += 2;
            out.push_back(static_cast<char>(high * 16 + low));
            break;
        }
        default:  // \\  \"  \{  \}  stand for themselves
            out.push_back(e);
            break;
        }
    }
    return out;
}

}  // namespace ferrum
