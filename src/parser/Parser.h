#ifndef FERRUM_PARSER_PARSER_H
#define FERRUM_PARSER_PARSER_H

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ast/AST.h"
#include "lexer/Token.h"

namespace ferrum {

/// A syntax error. line() and column() are 1-based; the column counts bytes.
/// what() returns "line:column: message"; the CLI prepends the file name.
class ParseError : public std::runtime_error {
public:
    ParseError(std::string message, std::uint32_t atLine, std::uint32_t atColumn);

    const std::string& message() const noexcept { return message_; }
    std::uint32_t line() const noexcept { return line_; }
    std::uint32_t column() const noexcept { return column_; }

private:
    std::string message_;
    std::uint32_t line_;
    std::uint32_t column_;
};

/// Recursive-descent parser for the grammar in SPEC.md, Appendix A.
///
/// - Newlines are not significant and `;` is optional: a stray `;` is an empty
///   statement and is skipped. Because of that, a statement that starts with
///   `(`, `[`, `-`, `*` or `&` continues the previous expression unless the
///   previous statement ends with `;` (SPEC.md section 2.2).
/// - Every node's `loc` is the position of the leftmost token of the construct
///   (for `palam` declarations, the `palam`).
/// - Parenthesized expressions leave no node of their own.
/// - Tuples and `discerne` are reserved: they are recognized only to report
///   that they are not available yet. `genus` is reserved the same way.
/// - The first syntax error is reported by throwing ParseError; after that the
///   Parser must not be used again.
/// - Nesting (parentheses, unary operators, types, blocks) is limited to 256
///   levels so that hostile input produces an error instead of exhausting the
///   stack.
class Parser {
public:
    /// `tokens` is the output of Lexer::tokenize(), ending with an Eof token
    /// (one is appended if missing). The source buffer the tokens point into
    /// only has to live until parseProgram() returns: the AST owns copies of
    /// all names and strings.
    explicit Parser(std::vector<Token> tokens);

    Program parseProgram();

private:
    struct DepthGuard;
    using ExprParseFn = ExprPtr (Parser::*)();

    // Token cursor
    const Token& peek(std::size_t offset = 0) const;
    const Token& current() const { return peek(0); }
    bool check(TokKind kind) const { return current().kind == kind; }
    bool match(TokKind kind);
    const Token& advance();
    const Token& expect(TokKind kind, const std::string& context);
    const Token& expectIdent(const std::string& what);

    [[noreturn]] void fail(const std::string& message, const Token& at) const;
    [[noreturn]] void failAt(const std::string& message, SourceLoc at) const;
    [[noreturn]] void failReserved(const Token& at) const;

    // Declarations
    StmtPtr parseTopLevelItem();
    StmtPtr parseImport();
    StmtPtr parseFunction(SourceLoc start, bool isPublic);
    StmtPtr parseExtern();
    StmtPtr parseStruct(SourceLoc start, bool isPublic);
    std::vector<Param> parseParams();

    // Statements
    std::unique_ptr<BlockStmt> parseBlock(const std::string& context);
    StmtPtr parseStatement();
    StmtPtr parseVarDecl(SourceLoc start, bool isPublic);
    StmtPtr parseReturn();
    StmtPtr parseExit();
    StmtPtr parseIf();
    StmtPtr parseWhile();
    StmtPtr parseFor();
    StmtPtr parseExpressionOrAssignment();

    // Types
    TypePtr parseType();

    // Expressions, lowest precedence first
    ExprPtr parseExpression();
    ExprPtr parseBinaryLevel(ExprParseFn next, std::initializer_list<std::pair<TokKind, BinaryOp>> ops);
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseComparison();
    ExprPtr parseBitOr();
    ExprPtr parseBitXor();
    ExprPtr parseBitAnd();
    ExprPtr parseShift();
    ExprPtr parseAdd();
    ExprPtr parseMul();
    ExprPtr parseCast();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();
    std::vector<ExprPtr> parseCallArguments();

    // Literals
    std::uint64_t parseIntegerLiteral(const Token& token) const;
    double parseFloatLiteral(const Token& token) const;

    static constexpr int kMaxDepth = 256;

    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
    int depth_ = 0;
};

/// Lexes and parses a whole source file. Throws LexError or ParseError.
/// The file is tokenized completely before parsing starts, so a lexical error
/// anywhere in the file is reported before any syntax error.
Program parse(std::string_view source);

}  // namespace ferrum

#endif  // FERRUM_PARSER_PARSER_H
