#include "parser/Parser.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <optional>

#include "lexer/Lexer.h"

namespace ferrum {

ParseError::ParseError(std::string message, std::uint32_t atLine, std::uint32_t atColumn)
    : std::runtime_error(std::to_string(atLine) + ":" + std::to_string(atColumn) + ": " + message),
      message_(std::move(message)),
      line_(atLine),
      column_(atColumn) {}

namespace {

SourceLoc locOf(const Token& token) {
    return SourceLoc{token.line, token.column};
}

/// How a token is named in an error message.
std::string describeToken(const Token& token) {
    switch (token.kind) {
    case TokKind::Eof:
        return "end of file";
    case TokKind::Ident:
        return "identifier '" + std::string(token.lexeme) + "'";
    case TokKind::IntLit:
    case TokKind::FloatLit:
        return "number '" + std::string(token.lexeme) + "'";
    case TokKind::StrLit:
        return "string literal";
    default:
        return "'" + std::string(tokKindSpelling(token.kind)) + "'";
    }
}

std::optional<BinaryOp> comparisonOp(TokKind kind) {
    switch (kind) {
    case TokKind::EqEq:
        return BinaryOp::Eq;
    case TokKind::NotEq:
        return BinaryOp::Ne;
    case TokKind::Lt:
        return BinaryOp::Lt;
    case TokKind::LtEq:
        return BinaryOp::Le;
    case TokKind::Gt:
        return BinaryOp::Gt;
    case TokKind::GtEq:
        return BinaryOp::Ge;
    default:
        return std::nullopt;
    }
}

std::optional<UnaryOp> unaryOpFor(TokKind kind) {
    switch (kind) {
    case TokKind::Minus:
        return UnaryOp::Neg;
    case TokKind::Bang:
        return UnaryOp::Not;
    case TokKind::Tilde:
        return UnaryOp::BitNot;
    case TokKind::Star:
        return UnaryOp::Deref;
    case TokKind::Amp:
        return UnaryOp::AddrOf;
    default:
        return std::nullopt;
    }
}

bool isAssignable(const Expr& expr) {
    if (isa<IndexExpr>(expr) || isa<MemberExpr>(expr)) {
        return true;
    }
    const auto* unary = dynCast<UnaryExpr>(&expr);
    return unary != nullptr && unary->op == UnaryOp::Deref;
}

bool startsAType(const Token& token) {
    switch (token.kind) {
    case TokKind::Ident:
    case TokKind::Star:
    case TokKind::Question:
    case TokKind::LBracket:
        return true;
    default:
        return isTypeName(token.kind);
    }
}

}  // namespace

struct Parser::DepthGuard {
    explicit DepthGuard(Parser& p) : parser(p) {
        if (++parser.depth_ > kMaxDepth) {
            parser.fail("nesting is too deep (the limit is " + std::to_string(kMaxDepth) + " levels)",
                        parser.current());
        }
    }
    DepthGuard(const DepthGuard&) = delete;
    DepthGuard& operator=(const DepthGuard&) = delete;
    ~DepthGuard() { --parser.depth_; }

    Parser& parser;
};

// ---------------------------------------------------------------------------
// Token cursor and errors
// ---------------------------------------------------------------------------

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {
    if (tokens_.empty() || tokens_.back().kind != TokKind::Eof) {
        Token eof;
        eof.kind = TokKind::Eof;
        if (!tokens_.empty()) {
            eof.line = tokens_.back().line;
            eof.column = tokens_.back().column;
        }
        tokens_.push_back(eof);
    }
}

const Token& Parser::peek(std::size_t offset) const {
    return tokens_[std::min(pos_ + offset, tokens_.size() - 1)];
}

bool Parser::match(TokKind kind) {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::advance() {
    const Token& token = tokens_[pos_];
    if (token.kind != TokKind::Eof) {
        ++pos_;
    }
    return token;
}

const Token& Parser::expect(TokKind kind, const std::string& context) {
    if (!check(kind)) {
        fail("expected '" + std::string(tokKindSpelling(kind)) + "' " + context + ", found " +
                 describeToken(current()),
             current());
    }
    return advance();
}

const Token& Parser::expectIdent(const std::string& what) {
    if (!check(TokKind::Ident)) {
        fail("expected " + what + ", found " + describeToken(current()), current());
    }
    return advance();
}

void Parser::fail(const std::string& message, const Token& at) const {
    throw ParseError(message, at.line, at.column);
}

void Parser::failAt(const std::string& message, SourceLoc at) const {
    throw ParseError(message, at.line, at.column);
}

void Parser::failReserved(const Token& at) const {
    if (at.kind == TokKind::KwDiscerne) {
        fail("pattern matching ('discerne') is reserved for a future version (Phase 6)", at);
    }
    fail("enumerations ('genus') are reserved for a future version (Phase 5)", at);
}

// ---------------------------------------------------------------------------
// Declarations
// ---------------------------------------------------------------------------

Program Parser::parseProgram() {
    Program program;
    while (!check(TokKind::Eof)) {
        if (match(TokKind::Semicolon)) {
            continue;
        }
        program.items.push_back(parseTopLevelItem());
    }
    return program;
}

StmtPtr Parser::parseTopLevelItem() {
    const Token& token = current();
    switch (token.kind) {
    case TokKind::KwImporta:
        return parseImport();
    case TokKind::KwOpus:
        return parseFunction(locOf(token), false);
    case TokKind::KwForma:
        return parseStruct(locOf(token), false);
    case TokKind::KwFixum:
        return parseVarDecl(locOf(token), false);
    case TokKind::KwForis:
        return parseExtern();
    case TokKind::KwPalam: {
        advance();
        switch (current().kind) {
        case TokKind::KwOpus:
            return parseFunction(locOf(token), true);
        case TokKind::KwForma:
            return parseStruct(locOf(token), true);
        case TokKind::KwFixum:
            return parseVarDecl(locOf(token), true);
        default:
            fail("expected 'opus', 'forma' or 'fixum' after 'palam', found " + describeToken(current()),
                 current());
        }
    }
    case TokKind::KwMut:
        fail("top-level variables must be declared with 'fixum'; 'mut' is only allowed inside functions",
             token);
    case TokKind::KwDiscerne:
    case TokKind::KwGenus:
        failReserved(token);
    default:
        fail("expected a declaration ('opus', 'forma', 'fixum', 'importa' or 'foris') at the top level, "
             "found " +
                 describeToken(token),
             token);
    }
}

StmtPtr Parser::parseImport() {
    const Token& keyword = advance();
    if (!check(TokKind::StrLit)) {
        fail("expected a string literal with the module path after 'importa', found " +
                 describeToken(current()),
             current());
    }
    const Token& path = advance();
    return std::make_unique<ImportDecl>(locOf(keyword), unescapeString(path.lexeme));
}

std::vector<Param> Parser::parseParams() {
    expect(TokKind::LParen, "after the function name");
    std::vector<Param> params;
    if (!check(TokKind::RParen)) {
        do {
            const Token& name = expectIdent("a parameter name");
            expect(TokKind::Colon, "after the parameter name (parameters are written 'name: type')");
            TypePtr type = parseType();
            params.push_back(Param{locOf(name), std::string(name.lexeme), std::move(type)});
        } while (match(TokKind::Comma) && !check(TokKind::RParen));
    }
    expect(TokKind::RParen, "to close the parameter list");
    return params;
}

StmtPtr Parser::parseFunction(SourceLoc start, bool isPublic) {
    advance();  // opus
    const Token& name = expectIdent("a function name after 'opus'");
    std::vector<Param> params = parseParams();

    TypePtr returnType;
    if (match(TokKind::Arrow)) {
        returnType = parseType();
    } else if (!check(TokKind::LBrace)) {
        std::string message =
            "expected '->' or '{' after the parameter list, found " + describeToken(current());
        if (startsAType(current())) {
            message += " (a return type is written '-> T')";
        }
        fail(message, current());
    }

    std::unique_ptr<BlockStmt> body = parseBlock("to start the function body");
    return std::make_unique<FuncDecl>(start, isPublic, false, std::string(name.lexeme), std::move(params),
                                      std::move(returnType), std::move(body));
}

StmtPtr Parser::parseExtern() {
    const Token& keyword = advance();  // foris
    expect(TokKind::KwOpus, "after 'foris'");
    const Token& name = expectIdent("a function name after 'opus'");
    std::vector<Param> params = parseParams();

    TypePtr returnType;
    if (match(TokKind::Arrow)) {
        returnType = parseType();
    }
    if (check(TokKind::LBrace)) {
        fail("a 'foris' function is only declared; it cannot have a body", current());
    }
    return std::make_unique<FuncDecl>(locOf(keyword), false, true, std::string(name.lexeme),
                                      std::move(params), std::move(returnType), nullptr);
}

StmtPtr Parser::parseStruct(SourceLoc start, bool isPublic) {
    advance();  // forma
    const Token& name = expectIdent("a type name after 'forma'");
    expect(TokKind::LBrace, "to start the field list");

    std::vector<Field> fields;
    while (!check(TokKind::RBrace)) {
        const Token& fieldName = expectIdent("a field name");
        expect(TokKind::Colon, "after the field name (fields are written 'name: type')");
        TypePtr type = parseType();
        fields.push_back(Field{locOf(fieldName), std::string(fieldName.lexeme), std::move(type)});
        if (!match(TokKind::Comma) && !check(TokKind::RBrace)) {
            fail("expected ',' or '}' after a field, found " + describeToken(current()), current());
        }
    }
    advance();  // }
    return std::make_unique<StructDecl>(start, isPublic, std::string(name.lexeme), std::move(fields));
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

std::unique_ptr<BlockStmt> Parser::parseBlock(const std::string& context) {
    DepthGuard guard(*this);
    const Token& open = expect(TokKind::LBrace, context);
    std::vector<StmtPtr> stmts;
    while (!check(TokKind::RBrace)) {
        if (check(TokKind::Eof)) {
            fail("expected '}' to close the block opened at " + std::to_string(open.line) + ":" +
                     std::to_string(open.column) + ", found end of file",
                 current());
        }
        if (match(TokKind::Semicolon)) {
            continue;
        }
        stmts.push_back(parseStatement());
    }
    advance();  // }
    return std::make_unique<BlockStmt>(locOf(open), std::move(stmts));
}

StmtPtr Parser::parseStatement() {
    const Token& token = current();
    switch (token.kind) {
    case TokKind::KwMut:
    case TokKind::KwFixum:
        return parseVarDecl(locOf(token), false);
    case TokKind::KwRedde:
        return parseReturn();
    case TokKind::KwExitus:
        return parseExit();
    case TokKind::KwRumpe:
        advance();
        return std::make_unique<BreakStmt>(locOf(token));
    case TokKind::KwPerge:
        advance();
        return std::make_unique<ContinueStmt>(locOf(token));
    case TokKind::KwSi:
        return parseIf();
    case TokKind::KwDum:
        return parseWhile();
    case TokKind::KwPer:
        return parseFor();
    case TokKind::KwAudax: {
        advance();
        return std::make_unique<AudaxStmt>(locOf(token), parseBlock("to start the 'audax' block"));
    }
    case TokKind::LBrace:
        return parseBlock("");
    case TokKind::KwAliter:
        fail("'aliter' without a matching 'si'", token);
    case TokKind::KwDiscerne:
    case TokKind::KwGenus:
        failReserved(token);
    case TokKind::KwOpus:
    case TokKind::KwForma:
    case TokKind::KwPalam:
    case TokKind::KwForis:
    case TokKind::KwImporta:
        fail("'" + std::string(tokKindSpelling(token.kind)) +
                 "' declarations are only allowed at the top level of a file",
             token);
    case TokKind::Ident:
        if (peek(1).kind == TokKind::Assign) {
            advance();  // name
            advance();  // =
            ExprPtr value = parseExpression();
            return std::make_unique<AssignOrDeclStmt>(locOf(token), std::string(token.lexeme),
                                                      std::move(value));
        }
        return parseExpressionOrAssignment();
    default:
        return parseExpressionOrAssignment();
    }
}

StmtPtr Parser::parseExpressionOrAssignment() {
    const SourceLoc start = locOf(current());
    ExprPtr expr = parseExpression();
    if (check(TokKind::Assign)) {
        const Token& equals = advance();
        if (!isAssignable(*expr)) {
            fail("invalid assignment target: only a variable, an index, a member or a dereference can be "
                 "assigned",
                 equals);
        }
        ExprPtr value = parseExpression();
        return std::make_unique<AssignStmt>(start, std::move(expr), std::move(value));
    }
    return std::make_unique<ExprStmt>(start, std::move(expr));
}

StmtPtr Parser::parseVarDecl(SourceLoc start, bool isPublic) {
    const Token& keyword = advance();  // mut | fixum
    const VarKind kind = keyword.kind == TokKind::KwMut ? VarKind::Mut : VarKind::Fixum;
    const Token& name =
        expectIdent("a variable name after '" + std::string(tokKindSpelling(keyword.kind)) + "'");

    TypePtr type;
    if (match(TokKind::Colon)) {
        type = parseType();
    }
    if (!check(TokKind::Assign)) {
        fail("expected '=': a variable declaration needs an initializer, found " + describeToken(current()),
             current());
    }
    advance();  // =
    ExprPtr init = parseExpression();
    return std::make_unique<VarDecl>(start, kind, std::string(name.lexeme), std::move(type), std::move(init),
                                     isPublic);
}

StmtPtr Parser::parseReturn() {
    const Token& keyword = advance();  // redde
    ExprPtr value;
    // `redde` takes no expression only before '}', ';' or the end of the file.
    if (!check(TokKind::RBrace) && !check(TokKind::Semicolon) && !check(TokKind::Eof)) {
        value = parseExpression();
    }
    return std::make_unique<ReturnStmt>(locOf(keyword), std::move(value));
}

StmtPtr Parser::parseExit() {
    const Token& keyword = advance();  // exitus
    ExprPtr expr = parseExpression();
    auto* call = dynCast<CallExpr>(expr.get());
    if (call == nullptr) {
        failAt("'exitus' must be followed by a call, as in 'exitus libera(p)'", expr->loc);
    }
    return std::make_unique<ExitStmt>(locOf(keyword), std::move(call->callee), std::move(call->args));
}

StmtPtr Parser::parseIf() {
    const Token& first = advance();  // si
    ExprPtr cond = parseExpression();
    std::unique_ptr<BlockStmt> thenBlock = parseBlock("to start the 'si' body");
    auto root = std::make_unique<IfStmt>(locOf(first), std::move(cond), std::move(thenBlock), nullptr);

    // `aliter si` chains are built in a loop, so a long chain does not nest the parser.
    IfStmt* tail = root.get();
    while (match(TokKind::KwAliter)) {
        if (check(TokKind::KwSi)) {
            const Token& si = advance();
            ExprPtr nextCond = parseExpression();
            std::unique_ptr<BlockStmt> nextBlock = parseBlock("to start the 'si' body");
            auto next =
                std::make_unique<IfStmt>(locOf(si), std::move(nextCond), std::move(nextBlock), nullptr);
            IfStmt* nextTail = next.get();
            tail->elseBranch = std::move(next);
            tail = nextTail;
        } else {
            tail->elseBranch = parseBlock("to start the 'aliter' body");
            break;
        }
    }
    return root;
}

StmtPtr Parser::parseWhile() {
    const Token& keyword = advance();  // dum
    ExprPtr cond = parseExpression();
    std::unique_ptr<BlockStmt> body = parseBlock("to start the 'dum' body");
    return std::make_unique<WhileStmt>(locOf(keyword), std::move(cond), std::move(body));
}

StmtPtr Parser::parseFor() {
    const Token& keyword = advance();  // per
    const Token& variable = expectIdent("a loop variable name after 'per'");
    expect(TokKind::KwIn, "after the loop variable");

    ExprPtr iterable = parseExpression();
    if (check(TokKind::DotDot) || check(TokKind::DotDotEq)) {
        const bool inclusive = advance().kind == TokKind::DotDotEq;
        ExprPtr end = parseExpression();
        const SourceLoc start = iterable->loc;
        iterable = std::make_unique<RangeExpr>(start, std::move(iterable), std::move(end), inclusive);
    }

    std::unique_ptr<BlockStmt> body = parseBlock("to start the 'per' body");
    return std::make_unique<ForStmt>(locOf(keyword), std::string(variable.lexeme), std::move(iterable),
                                     std::move(body));
}

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

TypePtr Parser::parseType() {
    DepthGuard guard(*this);
    const Token& token = current();
    const SourceLoc loc = locOf(token);
    switch (token.kind) {
    case TokKind::Star:
        advance();
        return std::make_unique<PointerType>(loc, parseType());
    case TokKind::Question:
        advance();
        return std::make_unique<OptionalType>(loc, parseType());
    case TokKind::LBracket:
        advance();
        expect(TokKind::RBracket, "in the slice type '[]T'");
        return std::make_unique<SliceType>(loc, parseType());
    case TokKind::Ident:
        advance();
        return std::make_unique<NamedType>(loc, std::string(token.lexeme));
    case TokKind::LParen:
        fail("tuple types are reserved for a future version (Phase 4)", token);
    default:
        if (isTypeName(token.kind)) {
            advance();
            return std::make_unique<NamedType>(loc, tokKindSpelling(token.kind));
        }
        fail("expected a type, found " + describeToken(token), token);
    }
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

ExprPtr Parser::parseExpression() {
    DepthGuard guard(*this);
    return parseOr();
}

ExprPtr Parser::parseBinaryLevel(ExprParseFn next,
                                 std::initializer_list<std::pair<TokKind, BinaryOp>> ops) {
    ExprPtr lhs = (this->*next)();
    for (;;) {
        std::optional<BinaryOp> found;
        for (const auto& entry : ops) {
            if (check(entry.first)) {
                found = entry.second;
                break;
            }
        }
        if (!found) {
            return lhs;
        }
        advance();
        ExprPtr rhs = (this->*next)();
        const SourceLoc loc = lhs->loc;
        lhs = std::make_unique<BinaryExpr>(loc, *found, std::move(lhs), std::move(rhs));
    }
}

ExprPtr Parser::parseOr() {
    return parseBinaryLevel(&Parser::parseAnd, {{TokKind::PipePipe, BinaryOp::Or}});
}

ExprPtr Parser::parseAnd() {
    return parseBinaryLevel(&Parser::parseComparison, {{TokKind::AmpAmp, BinaryOp::And}});
}

ExprPtr Parser::parseComparison() {
    ExprPtr lhs = parseBitOr();
    const std::optional<BinaryOp> op = comparisonOp(current().kind);
    if (!op) {
        return lhs;
    }
    advance();
    ExprPtr rhs = parseBitOr();
    if (comparisonOp(current().kind)) {
        fail("comparison operators cannot be chained; write 'a < b && b < c' instead", current());
    }
    const SourceLoc loc = lhs->loc;
    return std::make_unique<BinaryExpr>(loc, *op, std::move(lhs), std::move(rhs));
}

ExprPtr Parser::parseBitOr() {
    return parseBinaryLevel(&Parser::parseBitXor, {{TokKind::Pipe, BinaryOp::BitOr}});
}

ExprPtr Parser::parseBitXor() {
    return parseBinaryLevel(&Parser::parseBitAnd, {{TokKind::Caret, BinaryOp::BitXor}});
}

ExprPtr Parser::parseBitAnd() {
    return parseBinaryLevel(&Parser::parseShift, {{TokKind::Amp, BinaryOp::BitAnd}});
}

ExprPtr Parser::parseShift() {
    return parseBinaryLevel(&Parser::parseAdd,
                            {{TokKind::Shl, BinaryOp::Shl}, {TokKind::Shr, BinaryOp::Shr}});
}

ExprPtr Parser::parseAdd() {
    return parseBinaryLevel(&Parser::parseMul,
                            {{TokKind::Plus, BinaryOp::Add}, {TokKind::Minus, BinaryOp::Sub}});
}

ExprPtr Parser::parseMul() {
    return parseBinaryLevel(&Parser::parseCast, {{TokKind::Star, BinaryOp::Mul},
                                                 {TokKind::Slash, BinaryOp::Div},
                                                 {TokKind::Percent, BinaryOp::Rem}});
}

ExprPtr Parser::parseCast() {
    ExprPtr expr = parseUnary();
    while (match(TokKind::KwUt)) {
        TypePtr target = parseType();
        const SourceLoc loc = expr->loc;
        expr = std::make_unique<CastExpr>(loc, std::move(expr), std::move(target));
    }
    return expr;
}

ExprPtr Parser::parseUnary() {
    const std::optional<UnaryOp> op = unaryOpFor(current().kind);
    if (!op) {
        return parsePostfix();
    }
    DepthGuard guard(*this);
    const Token& token = advance();
    ExprPtr operand = parseUnary();
    return std::make_unique<UnaryExpr>(locOf(token), *op, std::move(operand));
}

ExprPtr Parser::parsePostfix() {
    ExprPtr expr = parsePrimary();
    for (;;) {
        const SourceLoc loc = expr->loc;
        if (match(TokKind::LParen)) {
            std::vector<ExprPtr> args = parseCallArguments();
            expr = std::make_unique<CallExpr>(loc, std::move(expr), std::move(args));
        } else if (match(TokKind::LBracket)) {
            ExprPtr index = parseExpression();
            expect(TokKind::RBracket, "to close the index");
            expr = std::make_unique<IndexExpr>(loc, std::move(expr), std::move(index));
        } else if (match(TokKind::Dot)) {
            const Token& member = expectIdent("a member name after '.'");
            expr = std::make_unique<MemberExpr>(loc, std::move(expr), std::string(member.lexeme));
        } else if (match(TokKind::Question)) {
            expr = std::make_unique<PropagateExpr>(loc, std::move(expr));
        } else {
            return expr;
        }
    }
}

std::vector<ExprPtr> Parser::parseCallArguments() {
    std::vector<ExprPtr> args;
    if (!check(TokKind::RParen)) {
        do {
            args.push_back(parseExpression());
        } while (match(TokKind::Comma) && !check(TokKind::RParen));
    }
    expect(TokKind::RParen, "to close the argument list");
    return args;
}

ExprPtr Parser::parsePrimary() {
    const Token& token = current();
    const SourceLoc loc = locOf(token);
    switch (token.kind) {
    case TokKind::IntLit:
        advance();
        return std::make_unique<IntLitExpr>(loc, parseIntegerLiteral(token));
    case TokKind::FloatLit:
        advance();
        return std::make_unique<FloatLitExpr>(loc, parseFloatLiteral(token));
    case TokKind::StrLit:
        advance();
        return std::make_unique<StrLitExpr>(loc, unescapeString(token.lexeme));
    case TokKind::KwVerum:
        advance();
        return std::make_unique<BoolLitExpr>(loc, true);
    case TokKind::KwFalsum:
        advance();
        return std::make_unique<BoolLitExpr>(loc, false);
    case TokKind::KwNihil:
        advance();
        return std::make_unique<NihilLitExpr>(loc);
    case TokKind::Ident:
        advance();
        return std::make_unique<IdentExpr>(loc, std::string(token.lexeme));
    case TokKind::KwCofre: {
        advance();
        expect(TokKind::LParen, "after 'cofre' (usage: cofre(T, n))");
        TypePtr elementType = parseType();
        expect(TokKind::Comma, "after the element type of 'cofre' (usage: cofre(T, n))");
        ExprPtr count = parseExpression();
        expect(TokKind::RParen, "to close 'cofre'");
        return std::make_unique<AllocExpr>(loc, std::move(elementType), std::move(count));
    }
    case TokKind::KwLibera: {
        advance();
        expect(TokKind::LParen, "after 'libera' (usage: libera(p))");
        ExprPtr pointer = parseExpression();
        expect(TokKind::RParen, "to close 'libera'; it takes exactly one argument");
        std::vector<ExprPtr> args;
        args.push_back(std::move(pointer));
        return std::make_unique<CallExpr>(loc, std::make_unique<BuiltinExpr>(loc, Builtin::Libera),
                                          std::move(args));
    }
    case TokKind::LParen: {
        advance();
        ExprPtr inner = parseExpression();
        if (check(TokKind::Comma)) {
            fail("tuple expressions are reserved for a future version (Phase 4)", current());
        }
        expect(TokKind::RParen, "to close the parenthesized expression");
        return inner;
    }
    case TokKind::KwDiscerne:
    case TokKind::KwGenus:
        failReserved(token);
    default:
        fail("expected an expression, found " + describeToken(token), token);
    }
}

// ---------------------------------------------------------------------------
// Literals
// ---------------------------------------------------------------------------

std::uint64_t Parser::parseIntegerLiteral(const Token& token) const {
    std::string_view text = token.lexeme;
    std::uint64_t base = 10;
    if (text.size() > 2 && text[0] == '0') {
        if (text[1] == 'x') {
            base = 16;
        } else if (text[1] == 'o') {
            base = 8;
        } else if (text[1] == 'b') {
            base = 2;
        }
        if (base != 10) {
            text.remove_prefix(2);
        }
    }

    constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t value = 0;
    for (const char c : text) {
        if (c == '_') {
            continue;
        }
        std::uint64_t digit = 0;
        if (c >= '0' && c <= '9') {
            digit = static_cast<std::uint64_t>(c - '0');
        } else {
            digit = static_cast<std::uint64_t>((c | 0x20) - 'a' + 10);
        }
        if (value > (kMax - digit) / base) {
            fail("integer literal is too large (it must fit in 64 bits)", token);
        }
        value = value * base + digit;
    }
    return value;
}

double Parser::parseFloatLiteral(const Token& token) const {
    std::string text;
    text.reserve(token.lexeme.size());
    for (const char c : token.lexeme) {
        if (c != '_') {
            text.push_back(c);
        }
    }
    const double value = std::strtod(text.c_str(), nullptr);
    if (std::isinf(value)) {
        fail("floating-point literal is out of range", token);
    }
    return value;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

Program parse(std::string_view source) {
    Parser parser(Lexer(source).tokenize());
    return parser.parseProgram();
}

}  // namespace ferrum
