#include "lexer/Token.h"

namespace ferrum {

namespace {

struct WordEntry {
    std::string_view text;
    TokKind kind;
};

constexpr WordEntry kWords[] = {
#define X(name, text) {text, TokKind::name},
    FERRUM_KEYWORD_TOKENS(X) FERRUM_TYPE_TOKENS(X)
#undef X
};

}  // namespace

const char* tokKindName(TokKind kind) {
    switch (kind) {
#define X(name, text) \
    case TokKind::name: \
        return #name;
        FERRUM_ALL_TOKENS(X)
#undef X
    }
    return "<invalid>";
}

const char* tokKindSpelling(TokKind kind) {
    switch (kind) {
#define X(name, text) \
    case TokKind::name: \
        return text;
        FERRUM_ALL_TOKENS(X)
#undef X
    }
    return "";
}

std::optional<TokKind> lookupWord(std::string_view word) {
    for (const WordEntry& entry : kWords) {
        if (entry.text == word) {
            return entry.kind;
        }
    }
    return std::nullopt;
}

}  // namespace ferrum
