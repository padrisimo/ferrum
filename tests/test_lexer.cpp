// Lexer tests: keywords, types, literals, operators, comments, positions and
// errors. A plain main(): returns 0 on success, 1 on any failed check.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "lexer/Lexer.h"

using ferrum::LexError;
using ferrum::Lexer;
using ferrum::TokKind;
using ferrum::Token;

namespace {

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                       \
        }                                                                     \
    } while (false)

std::vector<Token> lex(std::string_view src) {
    return Lexer(src).tokenize();
}

std::vector<TokKind> kinds(std::string_view src) {
    std::vector<TokKind> out;
    for (const Token& t : lex(src)) {
        out.push_back(t.kind);
    }
    return out;
}

/// Lexes `src` expecting exactly one token followed by Eof; returns that token.
Token single(std::string_view src) {
    const std::vector<Token> tokens = lex(src);
    CHECK(tokens.size() == 2);
    CHECK(tokens.back().kind == TokKind::Eof);
    return tokens.front();
}

/// True if lexing `src` throws a LexError at line:column whose message contains `part`.
bool failsAt(std::string_view src, std::uint32_t line, std::uint32_t column, std::string_view part) {
    try {
        lex(src);
    } catch (const LexError& e) {
        const bool ok = e.line() == line && e.column() == column &&
                        e.message().find(part) != std::string::npos;
        if (!ok) {
            std::fprintf(stderr, "  got %u:%u: %s\n", e.line(), e.column(), e.message().c_str());
        }
        return ok;
    }
    std::fprintf(stderr, "  no error for: %.*s\n", static_cast<int>(src.size()), src.data());
    return false;
}

void testKeywords() {
    const std::pair<const char*, TokKind> keywords[] = {
        {"opus", TokKind::KwOpus},       {"redde", TokKind::KwRedde},   {"exitus", TokKind::KwExitus},
        {"mut", TokKind::KwMut},         {"fixum", TokKind::KwFixum},   {"nihil", TokKind::KwNihil},
        {"verum", TokKind::KwVerum},     {"falsum", TokKind::KwFalsum}, {"si", TokKind::KwSi},
        {"aliter", TokKind::KwAliter},   {"dum", TokKind::KwDum},       {"per", TokKind::KwPer},
        {"in", TokKind::KwIn},           {"rumpe", TokKind::KwRumpe},   {"perge", TokKind::KwPerge},
        {"discerne", TokKind::KwDiscerne}, {"forma", TokKind::KwForma}, {"genus", TokKind::KwGenus},
        {"cofre", TokKind::KwCofre},     {"libera", TokKind::KwLibera}, {"audax", TokKind::KwAudax},
        {"importa", TokKind::KwImporta}, {"palam", TokKind::KwPalam},   {"foris", TokKind::KwForis},
        {"ut", TokKind::KwUt},
    };
    CHECK(sizeof(keywords) / sizeof(keywords[0]) == 25);
    for (const auto& [text, kind] : keywords) {
        const Token t = single(text);
        CHECK(t.kind == kind);
        CHECK(ferrum::isKeyword(t.kind));
        CHECK(!ferrum::isTypeName(t.kind));
        CHECK(t.lexeme == text);
        CHECK(std::string(ferrum::tokKindSpelling(kind)) == text);
    }
}

void testTypeNames() {
    const std::pair<const char*, TokKind> types[] = {
        {"i8", TokKind::TyI8},       {"i16", TokKind::TyI16},     {"i32", TokKind::TyI32},
        {"i64", TokKind::TyI64},     {"isize", TokKind::TyIsize}, {"u8", TokKind::TyU8},
        {"u16", TokKind::TyU16},     {"u32", TokKind::TyU32},     {"u64", TokKind::TyU64},
        {"usize", TokKind::TyUsize}, {"f32", TokKind::TyF32},     {"f64", TokKind::TyF64},
        {"bool", TokKind::TyBool},   {"str", TokKind::TyStr},     {"rawptr", TokKind::TyRawptr},
        {"vacuum", TokKind::TyVacuum},
    };
    CHECK(sizeof(types) / sizeof(types[0]) == 16);
    for (const auto& [text, kind] : types) {
        const Token t = single(text);
        CHECK(t.kind == kind);
        CHECK(ferrum::isTypeName(t.kind));
        CHECK(!ferrum::isKeyword(t.kind));
    }
}

void testIdentifiers() {
    // Case-sensitive, and a keyword prefix does not make a keyword.
    for (const char* name : {"Opus", "opusx", "opus_", "_x", "x1", "i33", "u7", "String", "main", "archivo", "_"}) {
        const Token t = single(name);
        CHECK(t.kind == TokKind::Ident);
        CHECK(t.lexeme == name);
    }
}

void testOperators() {
    const std::vector<TokKind> expected = {
        TokKind::Plus,   TokKind::Minus,    TokKind::Star,     TokKind::Slash,  TokKind::Percent,
        TokKind::Amp,    TokKind::Pipe,     TokKind::Caret,    TokKind::Tilde,  TokKind::Shl,
        TokKind::Shr,    TokKind::EqEq,     TokKind::NotEq,    TokKind::Lt,     TokKind::LtEq,
        TokKind::Gt,     TokKind::GtEq,     TokKind::AmpAmp,   TokKind::PipePipe, TokKind::Bang,
        TokKind::Assign, TokKind::DotDot,   TokKind::DotDotEq, TokKind::Dot,    TokKind::Comma,
        TokKind::Colon,  TokKind::Semicolon, TokKind::Arrow,   TokKind::Question, TokKind::LParen,
        TokKind::RParen, TokKind::LBrace,   TokKind::RBrace,   TokKind::LBracket, TokKind::RBracket,
        TokKind::Eof,
    };
    CHECK(kinds("+ - * / % & | ^ ~ << >> == != < <= > >= && || ! = .. ..= . , : ; -> ? ( ) { } [ ]") ==
          expected);

    // Without spaces, the longest operator wins.
    CHECK((kinds("<<=") == std::vector<TokKind>{TokKind::Shl, TokKind::Assign, TokKind::Eof}));
    CHECK((kinds("a<=b") == std::vector<TokKind>{TokKind::Ident, TokKind::LtEq, TokKind::Ident, TokKind::Eof}));
    CHECK((kinds("a->b") == std::vector<TokKind>{TokKind::Ident, TokKind::Arrow, TokKind::Ident, TokKind::Eof}));
    CHECK((kinds("a-b") == std::vector<TokKind>{TokKind::Ident, TokKind::Minus, TokKind::Ident, TokKind::Eof}));
    CHECK((kinds("...") == std::vector<TokKind>{TokKind::DotDot, TokKind::Dot, TokKind::Eof}));
    CHECK((kinds("?*u8") == std::vector<TokKind>{TokKind::Question, TokKind::Star, TokKind::TyU8, TokKind::Eof}));
    CHECK((kinds("[]i32") == std::vector<TokKind>{TokKind::LBracket, TokKind::RBracket, TokKind::TyI32, TokKind::Eof}));
    CHECK((kinds("a.b") == std::vector<TokKind>{TokKind::Ident, TokKind::Dot, TokKind::Ident, TokKind::Eof}));
}

void testRanges() {
    CHECK((kinds("0..10") == std::vector<TokKind>{TokKind::IntLit, TokKind::DotDot, TokKind::IntLit, TokKind::Eof}));
    CHECK((kinds("0..=10") == std::vector<TokKind>{TokKind::IntLit, TokKind::DotDotEq, TokKind::IntLit, TokKind::Eof}));
    CHECK((kinds("a..b") == std::vector<TokKind>{TokKind::Ident, TokKind::DotDot, TokKind::Ident, TokKind::Eof}));
    CHECK((kinds("1.5..3.5") == std::vector<TokKind>{TokKind::FloatLit, TokKind::DotDot, TokKind::FloatLit, TokKind::Eof}));
}

void testNumbers() {
    const std::pair<const char*, TokKind> numbers[] = {
        {"0", TokKind::IntLit},          {"42", TokKind::IntLit},          {"007", TokKind::IntLit},
        {"1_000_000", TokKind::IntLit},  {"0xFF", TokKind::IntLit},        {"0xdead_beef", TokKind::IntLit},
        {"0o17", TokKind::IntLit},       {"0b1010", TokKind::IntLit},      {"0b1_0", TokKind::IntLit},
        {"3.14", TokKind::FloatLit},     {"0.5", TokKind::FloatLit},       {"1e9", TokKind::FloatLit},
        {"2.5e-3", TokKind::FloatLit},   {"1E+5", TokKind::FloatLit},      {"1_0.2_5", TokKind::FloatLit},
        {"0x1e", TokKind::IntLit},  // 'e' is a hex digit here, not an exponent
    };
    for (const auto& [text, kind] : numbers) {
        const Token t = single(text);
        CHECK(t.kind == kind);
        CHECK(t.lexeme == text);
    }
    // A number has no sign: '-' is a separate token.
    CHECK((kinds("-5") == std::vector<TokKind>{TokKind::Minus, TokKind::IntLit, TokKind::Eof}));
    // "1." is an integer followed by a dot.
    CHECK((kinds("1.") == std::vector<TokKind>{TokKind::IntLit, TokKind::Dot, TokKind::Eof}));
}

void testStrings() {
    Token t = single("\"hello\"");
    CHECK(t.kind == TokKind::StrLit);
    CHECK(t.lexeme == "\"hello\"");
    CHECK(ferrum::unescapeString(t.lexeme) == "hello");

    CHECK(ferrum::unescapeString(single("\"\"").lexeme).empty());

    t = single(R"("a\nb\t\"q\"\\ \{ \} \x41\x7a \r")");
    CHECK(ferrum::unescapeString(t.lexeme) == "a\nb\t\"q\"\\ { } Az \r");

    t = single(R"("nul:\0:")");
    CHECK(ferrum::unescapeString(t.lexeme) == std::string("nul:\0:", 6));

    t = single(R"("\xFF")");
    CHECK(ferrum::unescapeString(t.lexeme) == std::string(1, static_cast<char>(0xFF)));

    // Well-formed UTF-8 is accepted as is.
    t = single("\"se\xC3\xB1" "or \xE2\x82\xAC \xF0\x9F\x98\x80\"");
    CHECK(t.kind == TokKind::StrLit);
    CHECK(ferrum::unescapeString(t.lexeme) == "se\xC3\xB1" "or \xE2\x82\xAC \xF0\x9F\x98\x80");

    // '#' and braces inside a string are plain text.
    t = single("\"# not a comment {x}\"");
    CHECK(ferrum::unescapeString(t.lexeme) == "# not a comment {x}");
}

void testComments() {
    CHECK((kinds("x # comment\ny") == std::vector<TokKind>{TokKind::Ident, TokKind::Ident, TokKind::Eof}));
    CHECK((kinds("## doc comment\nx") == std::vector<TokKind>{TokKind::Ident, TokKind::Eof}));
    CHECK((kinds("# only a comment, no newline") == std::vector<TokKind>{TokKind::Eof}));
    CHECK((kinds("# se\xC3\xB1" "or\nx") == std::vector<TokKind>{TokKind::Ident, TokKind::Eof}));
    CHECK((kinds("") == std::vector<TokKind>{TokKind::Eof}));
    CHECK((kinds("  \t\r\n  ") == std::vector<TokKind>{TokKind::Eof}));
}

void testPositions() {
    const std::vector<Token> t = lex("opus main()\n  redde\t1\r\nx");
    CHECK(t.size() == 8);
    CHECK(t[0].line == 1 && t[0].column == 1);  // opus
    CHECK(t[1].line == 1 && t[1].column == 6);  // main
    CHECK(t[2].line == 1 && t[2].column == 10);  // (
    CHECK(t[3].line == 1 && t[3].column == 11);  // )
    CHECK(t[4].line == 2 && t[4].column == 3);  // redde
    CHECK(t[5].line == 2 && t[5].column == 9);  // 1 (the tab counts as one column)
    CHECK(t[6].line == 3 && t[6].column == 1);  // x (after CRLF)
    CHECK(t[7].kind == TokKind::Eof && t[7].line == 3 && t[7].column == 2);
}

void testEofAndBom() {
    Lexer lexer("x");
    CHECK(lexer.next().kind == TokKind::Ident);
    CHECK(lexer.next().kind == TokKind::Eof);
    CHECK(lexer.next().kind == TokKind::Eof);

    const std::vector<Token> t = lex("\xEF\xBB\xBF" "opus");
    CHECK(t.size() == 2);
    CHECK(t[0].kind == TokKind::KwOpus && t[0].line == 1 && t[0].column == 1);
}

void testErrors() {
    CHECK(failsAt("\"abc", 1, 1, "unterminated string"));
    CHECK(failsAt("x = \"abc\n\"", 1, 5, "unterminated string"));
    CHECK(failsAt("\"abc\\", 1, 1, "unterminated string"));
    CHECK(failsAt("\"a\\qb\"", 1, 3, "invalid escape"));
    CHECK(failsAt("\"\\x4\"", 1, 2, "two hexadecimal digits"));
    CHECK(failsAt("\"\xFF\"", 1, 2, "invalid UTF-8"));
    CHECK(failsAt("\"\xC3\"", 1, 2, "invalid UTF-8"));  // truncated sequence
    CHECK(failsAt("\"\xC0\x80\"", 1, 2, "invalid UTF-8"));  // overlong encoding
    CHECK(failsAt("\"\xED\xA0\x80\"", 1, 2, "invalid UTF-8"));  // surrogate
    CHECK(failsAt(std::string_view("\"a\0b\"", 5), 1, 3, "NUL"));

    CHECK(failsAt("0b12", 1, 4, "invalid digit '2' in binary"));
    CHECK(failsAt("0o8", 1, 3, "invalid digit '8' in octal"));
    CHECK(failsAt("0xG", 1, 3, "invalid digit 'G' in hexadecimal"));
    CHECK(failsAt("0x", 1, 1, "hexadecimal literal has no digits"));
    CHECK(failsAt("0x_1", 1, 3, "between two digits"));
    CHECK(failsAt("10px", 1, 3, "invalid suffix"));
    CHECK(failsAt("1_", 1, 2, "between two digits"));
    CHECK(failsAt("1__0", 1, 2, "between two digits"));
    CHECK(failsAt("1_.5", 1, 2, "between two digits"));

    CHECK(failsAt("@", 1, 1, "unexpected character '@'"));
    CHECK(failsAt("a $", 1, 3, "unexpected character '$'"));
    CHECK(failsAt("'a'", 1, 1, "character literals"));
    CHECK(failsAt("contrase\xC3\xB1" "a", 1, 9, "non-ASCII"));
    CHECK(failsAt("x\n  \xE2\x82\xAC", 2, 3, "non-ASCII"));
    CHECK(failsAt(std::string_view("a\0b", 3), 1, 2, "0x00"));
    CHECK(failsAt("\x01", 1, 1, "0x01"));
    CHECK(failsAt("\\", 1, 1, "unexpected character '\\'"));

    // The error carries its position and a "line:column: message" what().
    try {
        lex("ok\n  @");
        CHECK(false);
    } catch (const LexError& e) {
        CHECK(std::string(e.what()) == "2:3: unexpected character '@'");
    }
}

void testSampleProgram() {
    // The sketch from README.md must tokenize.
    const std::string_view program = R"fe(
opus suma(a: i32, b: i32) -> i32 {
    redde a + b
}

opus main() -> i32 {
    fixum buffer = cofre(u8, 4096)   # typed heap allocation
    exitus libera(buffer)            # deferred; runs when the block ends

    mut intentos = 0
    dum intentos < 3 {
        intentos = intentos + 1
    }

    si suma(intentos, 1) > 3 {
        redde 0
    } aliter {
        redde 1
    }
}
)fe";
    const std::vector<Token> tokens = lex(program);
    CHECK(tokens.back().kind == TokKind::Eof);

    auto count = [&](TokKind kind) {
        std::size_t n = 0;
        for (const Token& t : tokens) {
            n += t.kind == kind ? 1U : 0U;
        }
        return n;
    };
    CHECK(count(TokKind::KwOpus) == 2);
    CHECK(count(TokKind::KwRedde) == 3);
    CHECK(count(TokKind::KwExitus) == 1);
    CHECK(count(TokKind::KwCofre) == 1);
    CHECK(count(TokKind::KwLibera) == 1);
    CHECK(count(TokKind::KwSi) == 1);
    CHECK(count(TokKind::KwAliter) == 1);
    CHECK(count(TokKind::KwDum) == 1);
    CHECK(count(TokKind::KwMut) == 1);
    CHECK(count(TokKind::KwFixum) == 1);
    CHECK(count(TokKind::Arrow) == 2);
    CHECK(count(TokKind::TyI32) == 4);
    CHECK(count(TokKind::LBrace) == count(TokKind::RBrace));
    CHECK(tokens.front().kind == TokKind::KwOpus && tokens.front().line == 2);
}

void testTokenNames() {
    CHECK(std::string(ferrum::tokKindName(TokKind::KwOpus)) == "KwOpus");
    CHECK(std::string(ferrum::tokKindName(TokKind::LBrace)) == "LBrace");
    CHECK(std::string(ferrum::tokKindName(TokKind::Eof)) == "Eof");
    CHECK(std::string(ferrum::tokKindSpelling(TokKind::LBrace)) == "{");
    CHECK(std::string(ferrum::tokKindSpelling(TokKind::DotDotEq)) == "..=");
    CHECK(std::string(ferrum::tokKindSpelling(TokKind::Ident)).empty());
    CHECK(ferrum::lookupWord("opus") == TokKind::KwOpus);
    CHECK(ferrum::lookupWord("vacuum") == TokKind::TyVacuum);
    CHECK(!ferrum::lookupWord("Opus").has_value());
    CHECK(!ferrum::lookupWord("").has_value());
}

}  // namespace

int main() {
    testKeywords();
    testTypeNames();
    testIdentifiers();
    testOperators();
    testRanges();
    testNumbers();
    testStrings();
    testComments();
    testPositions();
    testEofAndBom();
    testErrors();
    testSampleProgram();
    testTokenNames();

    if (failures != 0) {
        std::fprintf(stderr, "test_lexer: %d check(s) failed\n", failures);
        return 1;
    }
    std::printf("test_lexer: all checks passed\n");
    return 0;
}
