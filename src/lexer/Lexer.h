#ifndef FERRUM_LEXER_LEXER_H
#define FERRUM_LEXER_LEXER_H

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lexer/Token.h"

namespace ferrum {

/// A lexical error. line() and column() are 1-based; the column counts bytes.
/// what() returns "line:column: message"; the CLI prepends the file name.
class LexError : public std::runtime_error {
public:
    LexError(std::string message, std::uint32_t atLine, std::uint32_t atColumn);

    const std::string& message() const noexcept { return message_; }
    std::uint32_t line() const noexcept { return line_; }
    std::uint32_t column() const noexcept { return column_; }

private:
    std::string message_;
    std::uint32_t line_;
    std::uint32_t column_;
};

/// Turns Ferrum source text (SPEC.md section 2) into tokens.
///
/// The source buffer must outlive the Lexer and every Token it produces, since
/// tokens hold views into it. A UTF-8 byte order mark at the start is skipped.
/// The first lexical error is reported by throwing LexError; after that the
/// Lexer must not be used again.
class Lexer {
public:
    explicit Lexer(std::string_view source);

    /// Returns the next token. At the end of the input it returns an Eof
    /// token, and keeps returning Eof if called again.
    Token next();

    /// Returns every remaining token, ending with the Eof token.
    std::vector<Token> tokenize();

private:
    bool atEnd() const;
    char peek(std::size_t offset = 0) const;
    char advance();
    bool match(char expected);
    void skipTrivia();

    Token make(TokKind kind, std::size_t start, std::uint32_t line, std::uint32_t column) const;
    Token lexWord(std::size_t start, std::uint32_t line, std::uint32_t column);
    Token lexNumber(std::size_t start, std::uint32_t line, std::uint32_t column);
    Token lexString(std::size_t start, std::uint32_t line, std::uint32_t column);

    std::string_view src_;
    std::size_t pos_ = 0;
    std::uint32_t line_ = 1;
    std::uint32_t column_ = 1;
};

/// Decodes the text of a StrLit token: removes the quotes and resolves the
/// escapes. The argument must be the lexeme of a token produced by Lexer.
std::string unescapeString(std::string_view stringLexeme);

}  // namespace ferrum

#endif  // FERRUM_LEXER_LEXER_H
