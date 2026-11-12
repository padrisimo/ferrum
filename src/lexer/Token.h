#ifndef FERRUM_LEXER_TOKEN_H
#define FERRUM_LEXER_TOKEN_H

#include <cstdint>
#include <optional>
#include <string_view>

namespace ferrum {

// Single source of truth for every token kind: X(EnumName, "spelling").
// The spelling is empty for kinds whose text varies (identifiers, literals, EOF).

#define FERRUM_SPECIAL_TOKENS(X) \
    X(Eof, "")                   \
    X(Ident, "")                 \
    X(IntLit, "")                \
    X(FloatLit, "")              \
    X(StrLit, "")

// The 25 keywords of SPEC.md section 2.5. Keep KwOpus first and KwUt last:
// isKeyword() relies on that order.
#define FERRUM_KEYWORD_TOKENS(X) \
    X(KwOpus, "opus")            \
    X(KwRedde, "redde")          \
    X(KwExitus, "exitus")        \
    X(KwMut, "mut")              \
    X(KwFixum, "fixum")          \
    X(KwNihil, "nihil")          \
    X(KwVerum, "verum")          \
    X(KwFalsum, "falsum")        \
    X(KwSi, "si")                \
    X(KwAliter, "aliter")        \
    X(KwDum, "dum")              \
    X(KwPer, "per")              \
    X(KwIn, "in")                \
    X(KwRumpe, "rumpe")          \
    X(KwPerge, "perge")          \
    X(KwDiscerne, "discerne")    \
    X(KwForma, "forma")          \
    X(KwGenus, "genus")          \
    X(KwCofre, "cofre")          \
    X(KwLibera, "libera")        \
    X(KwAudax, "audax")          \
    X(KwImporta, "importa")      \
    X(KwPalam, "palam")          \
    X(KwForis, "foris")          \
    X(KwUt, "ut")

// The 16 primitive type names of SPEC.md section 3.1. Keep TyI8 first and
// TyVacuum last: isTypeName() relies on that order.
#define FERRUM_TYPE_TOKENS(X) \
    X(TyI8, "i8")             \
    X(TyI16, "i16")           \
    X(TyI32, "i32")           \
    X(TyI64, "i64")           \
    X(TyIsize, "isize")       \
    X(TyU8, "u8")             \
    X(TyU16, "u16")           \
    X(TyU32, "u32")           \
    X(TyU64, "u64")           \
    X(TyUsize, "usize")       \
    X(TyF32, "f32")           \
    X(TyF64, "f64")           \
    X(TyBool, "bool")         \
    X(TyStr, "str")           \
    X(TyRawptr, "rawptr")     \
    X(TyVacuum, "vacuum")

// Operators and punctuation of SPEC.md section 2.7.
#define FERRUM_PUNCT_TOKENS(X) \
    X(Plus, "+")               \
    X(Minus, "-")              \
    X(Star, "*")               \
    X(Slash, "/")              \
    X(Percent, "%")            \
    X(Amp, "&")                \
    X(Pipe, "|")               \
    X(Caret, "^")              \
    X(Tilde, "~")              \
    X(Shl, "<<")               \
    X(Shr, ">>")               \
    X(EqEq, "==")              \
    X(NotEq, "!=")             \
    X(Lt, "<")                 \
    X(LtEq, "<=")              \
    X(Gt, ">")                 \
    X(GtEq, ">=")              \
    X(AmpAmp, "&&")            \
    X(PipePipe, "||")          \
    X(Bang, "!")               \
    X(Assign, "=")             \
    X(DotDot, "..")            \
    X(DotDotEq, "..=")         \
    X(Dot, ".")                \
    X(Comma, ",")              \
    X(Colon, ":")              \
    X(Semicolon, ";")          \
    X(Arrow, "->")             \
    X(Question, "?")           \
    X(LParen, "(")             \
    X(RParen, ")")             \
    X(LBrace, "{")             \
    X(RBrace, "}")             \
    X(LBracket, "[")           \
    X(RBracket, "]")

#define FERRUM_ALL_TOKENS(X) \
    FERRUM_SPECIAL_TOKENS(X) \
    FERRUM_KEYWORD_TOKENS(X) \
    FERRUM_TYPE_TOKENS(X)    \
    FERRUM_PUNCT_TOKENS(X)

enum class TokKind : std::uint8_t {
#define X(name, text) name,
    FERRUM_ALL_TOKENS(X)
#undef X
};

inline constexpr int kKeywordCount =
    static_cast<int>(TokKind::KwUt) - static_cast<int>(TokKind::KwOpus) + 1;
inline constexpr int kTypeNameCount =
    static_cast<int>(TokKind::TyVacuum) - static_cast<int>(TokKind::TyI8) + 1;

static_assert(kKeywordCount == 25, "SPEC.md defines exactly 25 keywords");
static_assert(kTypeNameCount == 16, "SPEC.md defines exactly 16 primitive types");

constexpr bool isKeyword(TokKind k) {
    return k >= TokKind::KwOpus && k <= TokKind::KwUt;
}

constexpr bool isTypeName(TokKind k) {
    return k >= TokKind::TyI8 && k <= TokKind::TyVacuum;
}

/// The enumerator's name, e.g. "KwOpus" or "LBrace". Used by --dump-tokens.
const char* tokKindName(TokKind kind);

/// The fixed source text of a keyword, type name or operator, e.g. "opus" or
/// "{". Empty for Eof, identifiers and literals. Used in diagnostics.
const char* tokKindSpelling(TokKind kind);

/// Maps a word to its keyword or primitive-type token; empty for a plain
/// identifier. Matching is case-sensitive.
std::optional<TokKind> lookupWord(std::string_view word);

struct Token {
    TokKind kind = TokKind::Eof;
    /// The exact source text. A view into the source buffer given to the
    /// Lexer, which must outlive every token. Empty for Eof. String literals
    /// include their quotes and keep their escapes undecoded.
    std::string_view lexeme;
    /// 1-based position of the first byte of the token.
    std::uint32_t line = 1;
    std::uint32_t column = 1;
};

}  // namespace ferrum

#endif  // FERRUM_LEXER_TOKEN_H
