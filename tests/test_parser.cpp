// Parser tests: precedence, every statement and declaration form, locations,
// reserved syntax and error reporting. A plain main(): returns 0 on success,
// 1 on any failed check.
//
// Trees are compared as compact S-expressions produced by the small dumpers
// below, so each case is one line.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "lexer/Lexer.h"
#include "parser/Parser.h"

using namespace ferrum;

namespace {

int failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                               \
        }                                                                             \
    } while (false)

/// CHECK_EQ prints both sides when they differ.
#define CHECK_EQ(actual, expected)                                                         \
    do {                                                                                   \
        const auto actualValue = (actual);                                                 \
        const auto expectedValue = (expected);                                             \
        if (!(actualValue == expectedValue)) {                                             \
            std::fprintf(stderr, "%s:%d: CHECK_EQ failed: %s\n  actual:   %s\n  expected: %s\n", \
                         __FILE__, __LINE__, #actual, show(actualValue).c_str(),           \
                         show(expectedValue).c_str());                                     \
            ++failures;                                                                    \
        }                                                                                  \
    } while (false)

std::string show(const std::string& s) {
    return s;
}

std::string show(std::size_t n) {
    return std::to_string(n);
}

std::string show(const std::vector<std::string>& v) {
    std::string out = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        out += (i == 0 ? "" : ", ") + v[i];
    }
    return out + "]";
}

// ---------------------------------------------------------------------------
// S-expression dumpers (test-only; the real printer arrives with the CLI)
// ---------------------------------------------------------------------------

std::string typeStr(const TypeNode& t) {
    switch (t.kind) {
    case TypeKind::Named:
        return cast<NamedType>(t).name;
    case TypeKind::Pointer:
        return "*" + typeStr(*cast<PointerType>(t).pointee);
    case TypeKind::Optional:
        return "?" + typeStr(*cast<OptionalType>(t).inner);
    case TypeKind::Slice:
        return "[]" + typeStr(*cast<SliceType>(t).element);
    }
    return "<type?>";
}

const char* unaryName(UnaryOp op) {
    switch (op) {
    case UnaryOp::Neg:
        return "neg";
    case UnaryOp::Not:
        return "not";
    case UnaryOp::BitNot:
        return "bnot";
    case UnaryOp::Deref:
        return "deref";
    case UnaryOp::AddrOf:
        return "addr";
    }
    return "?";
}

std::string exprStr(const Expr& e) {
    switch (e.kind) {
    case ExprKind::IntLit:
        return std::to_string(cast<IntLitExpr>(e).value);
    case ExprKind::FloatLit: {
        char buffer[64];
        std::snprintf(buffer, sizeof buffer, "%g", cast<FloatLitExpr>(e).value);
        return buffer;
    }
    case ExprKind::StrLit:
        return "\"" + cast<StrLitExpr>(e).value + "\"";
    case ExprKind::BoolLit:
        return cast<BoolLitExpr>(e).value ? "verum" : "falsum";
    case ExprKind::NihilLit:
        return "nihil";
    case ExprKind::Ident:
        return cast<IdentExpr>(e).name;
    case ExprKind::Builtin:
        return builtinName(cast<BuiltinExpr>(e).which);
    case ExprKind::Unary: {
        const auto& u = cast<UnaryExpr>(e);
        return std::string("(") + unaryName(u.op) + " " + exprStr(*u.operand) + ")";
    }
    case ExprKind::Binary: {
        const auto& b = cast<BinaryExpr>(e);
        return std::string("(") + binaryOpSpelling(b.op) + " " + exprStr(*b.lhs) + " " + exprStr(*b.rhs) + ")";
    }
    case ExprKind::Cast: {
        const auto& c = cast<CastExpr>(e);
        return "(ut " + exprStr(*c.operand) + " " + typeStr(*c.target) + ")";
    }
    case ExprKind::Call: {
        const auto& c = cast<CallExpr>(e);
        std::string out = "(call " + exprStr(*c.callee);
        for (const ExprPtr& arg : c.args) {
            out += " " + exprStr(*arg);
        }
        return out + ")";
    }
    case ExprKind::Index: {
        const auto& i = cast<IndexExpr>(e);
        return "(idx " + exprStr(*i.base) + " " + exprStr(*i.index) + ")";
    }
    case ExprKind::Member: {
        const auto& m = cast<MemberExpr>(e);
        return "(. " + exprStr(*m.base) + " " + m.member + ")";
    }
    case ExprKind::Propagate:
        return "(? " + exprStr(*cast<PropagateExpr>(e).operand) + ")";
    case ExprKind::Alloc: {
        const auto& a = cast<AllocExpr>(e);
        return "(cofre " + typeStr(*a.elementType) + " " + exprStr(*a.count) + ")";
    }
    case ExprKind::Range: {
        const auto& r = cast<RangeExpr>(e);
        return std::string(r.inclusive ? "(..= " : "(.. ") + exprStr(*r.start) + " " + exprStr(*r.end) + ")";
    }
    }
    return "<expr?>";
}

std::string stmtStr(const Stmt& s);

std::string blockStr(const BlockStmt& b) {
    std::string out = "(block";
    for (const StmtPtr& stmt : b.stmts) {
        out += " " + stmtStr(*stmt);
    }
    return out + ")";
}

std::string stmtStr(const Stmt& s) {
    switch (s.kind) {
    case StmtKind::VarDecl: {
        const auto& v = cast<VarDecl>(s);
        std::string out = std::string("(") + (v.isPublic ? "pub " : "") + (v.varKind == VarKind::Mut ? "mut " : "fixum ") + v.name;
        if (v.type) {
            out += ":" + typeStr(*v.type);
        }
        return out + " " + exprStr(*v.init) + ")";
    }
    case StmtKind::AssignOrDecl: {
        const auto& a = cast<AssignOrDeclStmt>(s);
        return "(set " + a.name + " " + exprStr(*a.value) + ")";
    }
    case StmtKind::Assign: {
        const auto& a = cast<AssignStmt>(s);
        return "(assign " + exprStr(*a.target) + " " + exprStr(*a.value) + ")";
    }
    case StmtKind::Return: {
        const auto& r = cast<ReturnStmt>(s);
        return r.value ? "(redde " + exprStr(*r.value) + ")" : "(redde)";
    }
    case StmtKind::Exit: {
        const auto& x = cast<ExitStmt>(s);
        std::string out = "(exitus " + exprStr(*x.callee);
        for (const ExprPtr& arg : x.args) {
            out += " " + exprStr(*arg);
        }
        return out + ")";
    }
    case StmtKind::Break:
        return "(rumpe)";
    case StmtKind::Continue:
        return "(perge)";
    case StmtKind::If: {
        const auto& i = cast<IfStmt>(s);
        std::string out = "(si " + exprStr(*i.cond) + " " + blockStr(*i.thenBlock);
        if (i.elseBranch) {
            out += " " + stmtStr(*i.elseBranch);
        }
        return out + ")";
    }
    case StmtKind::While: {
        const auto& w = cast<WhileStmt>(s);
        return "(dum " + exprStr(*w.cond) + " " + blockStr(*w.body) + ")";
    }
    case StmtKind::For: {
        const auto& f = cast<ForStmt>(s);
        return "(per " + f.var + " " + exprStr(*f.iterable) + " " + blockStr(*f.body) + ")";
    }
    case StmtKind::Block:
        return blockStr(cast<BlockStmt>(s));
    case StmtKind::Audax:
        return "(audax " + blockStr(*cast<AudaxStmt>(s).body) + ")";
    case StmtKind::Expr:
        return "(expr " + exprStr(*cast<ExprStmt>(s).expr) + ")";
    case StmtKind::FuncDecl: {
        const auto& f = cast<FuncDecl>(s);
        std::string out = "(";
        out += f.isExtern ? "foris " : (f.isPublic ? "pub " : "");
        out += "opus " + f.name + " (";
        for (std::size_t i = 0; i < f.params.size(); ++i) {
            out += (i == 0 ? "" : " ") + f.params[i].name + ":" + typeStr(*f.params[i].type);
        }
        out += ")";
        if (f.returnType) {
            out += " -> " + typeStr(*f.returnType);
        }
        if (f.body) {
            out += " " + blockStr(*f.body);
        }
        return out + ")";
    }
    case StmtKind::StructDecl: {
        const auto& d = cast<StructDecl>(s);
        std::string out = std::string("(") + (d.isPublic ? "pub " : "") + "forma " + d.name;
        for (const Field& field : d.fields) {
            out += " " + field.name + ":" + typeStr(*field.type);
        }
        return out + ")";
    }
    case StmtKind::ImportDecl:
        return "(importa \"" + cast<ImportDecl>(s).path + "\")";
    }
    return "<stmt?>";
}

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

std::vector<std::string> items(const std::string& source) {
    const Program program = parse(source);
    std::vector<std::string> out;
    for (const StmtPtr& item : program.items) {
        out.push_back(stmtStr(*item));
    }
    return out;
}

/// The expression `source`, as an S-expression.
std::string expr(const std::string& source) {
    const Program program = parse("fixum v = " + source);
    return exprStr(*cast<VarDecl>(*program.items.at(0)).init);
}

/// The type `source`, as a string.
std::string type(const std::string& source) {
    const Program program = parse("fixum v: " + source + " = 0");
    return typeStr(*cast<VarDecl>(*program.items.at(0)).type);
}

/// The statements of a function body made of `source`.
std::vector<std::string> stmts(const std::string& source) {
    const Program program = parse("opus f() {" + source + "}");
    std::vector<std::string> out;
    for (const StmtPtr& stmt : cast<FuncDecl>(*program.items.at(0)).body->stmts) {
        out.push_back(stmtStr(*stmt));
    }
    return out;
}

/// True if parsing `source` throws a ParseError at line:column whose message contains `part`.
bool failsAt(const std::string& source, std::uint32_t line, std::uint32_t column, const std::string& part) {
    try {
        parse(source);
    } catch (const ParseError& e) {
        const bool ok = e.line() == line && e.column() == column &&
                        e.message().find(part) != std::string::npos;
        if (!ok) {
            std::fprintf(stderr, "  source: %s\n  got %u:%u: %s\n  want %u:%u containing: %s\n", source.c_str(),
                         e.line(), e.column(), e.message().c_str(), line, column, part.c_str());
        }
        return ok;
    } catch (const LexError& e) {
        std::fprintf(stderr, "  source: %s\n  unexpected LexError %u:%u: %s\n", source.c_str(), e.line(),
                     e.column(), e.message().c_str());
        return false;
    }
    std::fprintf(stderr, "  no error for: %s\n", source.c_str());
    return false;
}

/// Like failsAt, for a statement inside a function body. `column` is 1-based
/// within `statement` (which must be on one line).
bool stmtFailsAt(const std::string& statement, std::uint32_t column, const std::string& part) {
    const std::string prefix = "opus f() {";
    return failsAt(prefix + statement + "}", 1, static_cast<std::uint32_t>(prefix.size()) + column, part);
}

/// Like failsAt, for an expression after `fixum v = `.
bool exprFailsAt(const std::string& expression, std::uint32_t column, const std::string& part) {
    const std::string prefix = "fixum v = ";
    return failsAt(prefix + expression, 1, static_cast<std::uint32_t>(prefix.size()) + column, part);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

void testPrecedence() {
    CHECK_EQ(expr("a + b * c"), "(+ a (* b c))");
    CHECK_EQ(expr("a * b + c"), "(+ (* a b) c)");
    CHECK_EQ(expr("a - b - c"), "(- (- a b) c)");
    CHECK_EQ(expr("a / b % c"), "(% (/ a b) c)");
    CHECK_EQ(expr("a || b && c"), "(|| a (&& b c))");
    CHECK_EQ(expr("a && b || c"), "(|| (&& a b) c)");
    CHECK_EQ(expr("a & b == c"), "(== (& a b) c)");
    CHECK_EQ(expr("a | b ^ c & d"), "(| a (^ b (& c d)))");
    CHECK_EQ(expr("a << 1 + 2"), "(<< a (+ 1 2))");
    CHECK_EQ(expr("a >> 1 << 2"), "(<< (>> a 1) 2)");
    CHECK_EQ(expr("a < b + 1"), "(< a (+ b 1))");
    CHECK_EQ(expr("a == b && c != d"), "(&& (== a b) (!= c d))");
    CHECK_EQ(expr("(a + b) * c"), "(* (+ a b) c)");
    CHECK_EQ(expr("((a))"), "a");
}

void testUnaryAndCast() {
    CHECK_EQ(expr("-a * b"), "(* (neg a) b)");
    CHECK_EQ(expr("!a && b"), "(&& (not a) b)");
    CHECK_EQ(expr("~a & b"), "(& (bnot a) b)");
    CHECK_EQ(expr("*p + 1"), "(+ (deref p) 1)");
    CHECK_EQ(expr("**p"), "(deref (deref p))");
    CHECK_EQ(expr("&x"), "(addr x)");
    CHECK_EQ(expr("- - a"), "(neg (neg a))");
    CHECK_EQ(expr("a * *p"), "(* a (deref p))");
    CHECK_EQ(expr("a - -b"), "(- a (neg b))");
    CHECK_EQ(expr("a & &b"), "(& a (addr b))");

    // `ut` binds tighter than multiplication and looser than unary operators.
    CHECK_EQ(expr("a ut i64 + 1"), "(+ (ut a i64) 1)");
    CHECK_EQ(expr("a * b ut i64"), "(* a (ut b i64))");
    CHECK_EQ(expr("-a ut i64"), "(ut (neg a) i64)");
    CHECK_EQ(expr("a ut i32 ut i64"), "(ut (ut a i32) i64)");
    CHECK_EQ(expr("p ut *u8"), "(ut p *u8)");
    CHECK_EQ(expr("n ut f64 / 2"), "(/ (ut n f64) 2)");
}

void testComparisons() {
    CHECK_EQ(expr("a < b && b < c"), "(&& (< a b) (< b c))");
    CHECK_EQ(expr("(a < b) < c"), "(< (< a b) c)");
    CHECK_EQ(expr("a <= b"), "(<= a b)");
    CHECK_EQ(expr("a >= b"), "(>= a b)");

    // Chained comparisons are an error, reported at the second operator.
    CHECK(exprFailsAt("a < b < c", 7, "cannot be chained"));
    CHECK(exprFailsAt("a == b == c", 8, "cannot be chained"));
    CHECK(exprFailsAt("a < b == c", 7, "cannot be chained"));
    CHECK(exprFailsAt("a < b + 1 > c", 11, "cannot be chained"));
}

void testPostfix() {
    CHECK_EQ(expr("f()"), "(call f)");
    CHECK_EQ(expr("f(a)(b)"), "(call (call f a) b)");
    CHECK_EQ(expr("f(a + 1, g(b))"), "(call f (+ a 1) (call g b))");
    CHECK_EQ(expr("f(a, b,)"), "(call f a b)");
    CHECK_EQ(expr("(f)(x)"), "(call f x)");
    CHECK_EQ(expr("a.b.c"), "(. (. a b) c)");
    CHECK_EQ(expr("a[i][j]"), "(idx (idx a i) j)");
    CHECK_EQ(expr("a[i + 1]"), "(idx a (+ i 1))");
    CHECK_EQ(expr("a.b(c)[d]"), "(idx (call (. a b) c) d)");
    CHECK_EQ(expr("f(x)?"), "(? (call f x))");
    CHECK_EQ(expr("a.b?"), "(? (. a b))");
    CHECK_EQ(expr("-f(x)"), "(neg (call f x))");
    CHECK_EQ(expr("*p.x"), "(deref (. p x))");
    CHECK_EQ(expr("&a[0]"), "(addr (idx a 0))");
}

void testLiterals() {
    CHECK_EQ(expr("42"), "42");
    CHECK_EQ(expr("0xFF"), "255");
    CHECK_EQ(expr("0o17"), "15");
    CHECK_EQ(expr("0b101"), "5");
    CHECK_EQ(expr("1_000"), "1000");
    CHECK_EQ(expr("007"), "7");
    CHECK_EQ(expr("18446744073709551615"), "18446744073709551615");
    CHECK_EQ(expr("0xFFFFFFFFFFFFFFFF"), "18446744073709551615");
    CHECK_EQ(expr("2.5"), "2.5");
    CHECK_EQ(expr("1e3"), "1000");
    CHECK_EQ(expr("1_0.5"), "10.5");
    CHECK_EQ(expr("verum"), "verum");
    CHECK_EQ(expr("falsum"), "falsum");
    CHECK_EQ(expr("nihil"), "nihil");
    CHECK_EQ(expr("\"hi\""), "\"hi\"");
    CHECK_EQ(expr(R"fe("a\x41\{b")fe"), "\"aA{b\"");
    CHECK_EQ(expr("-5"), "(neg 5)");

    CHECK(exprFailsAt("18446744073709551616", 1, "too large"));
    CHECK(exprFailsAt("0x10000000000000000", 1, "too large"));
    CHECK(exprFailsAt("1e999", 1, "out of range"));

    // Values land in the AST as numbers, not text.
    const Program program = parse("fixum v = 0x10");
    CHECK(cast<IntLitExpr>(*cast<VarDecl>(*program.items[0]).init).value == 16);
}

void testMemoryExpressions() {
    CHECK_EQ(expr("cofre(u8, 4096)"), "(cofre u8 4096)");
    CHECK_EQ(expr("cofre(*u8, 2 * n)"), "(cofre *u8 (* 2 n))");
    CHECK_EQ(expr("cofre(Punto, 3)"), "(cofre Punto 3)");
    CHECK_EQ(expr("libera(p)"), "(call libera p)");
    CHECK_EQ(expr("libera(a.b)"), "(call libera (. a b))");

    CHECK(exprFailsAt("libera", 7, "after 'libera'"));
    CHECK(exprFailsAt("libera()", 8, "expected an expression"));
    CHECK(exprFailsAt("libera(a, b)", 9, "exactly one argument"));
    CHECK(exprFailsAt("cofre(u8)", 9, "after the element type"));
    CHECK(exprFailsAt("cofre(1, 2)", 7, "expected a type"));
    CHECK(exprFailsAt("cofre u8", 7, "after 'cofre'"));
}

void testTypes() {
    for (const char* name : {"i32", "u8", "f64", "bool", "str", "rawptr", "vacuum", "isize", "Punto"}) {
        CHECK_EQ(type(name), name);
    }
    CHECK_EQ(type("*u8"), "*u8");
    CHECK_EQ(type("?i32"), "?i32");
    CHECK_EQ(type("?*u8"), "?*u8");
    CHECK_EQ(type("[]i32"), "[]i32");
    CHECK_EQ(type("[]*u8"), "[]*u8");
    CHECK_EQ(type("*[]str"), "*[]str");
    CHECK_EQ(type("??i32"), "??i32");
    CHECK_EQ(type("[]?*Punto"), "[]?*Punto");

    CHECK(failsAt("fixum v: [i32] = 0", 1, 11, "slice type"));
    CHECK(failsAt("fixum v: = 0", 1, 10, "expected a type"));
    CHECK(failsAt("fixum v: 5 = 0", 1, 10, "expected a type"));
}

void testSimpleStatements() {
    CHECK_EQ(stmts("mut x = 1"), std::vector<std::string>{"(mut x 1)"});
    CHECK_EQ(stmts("fixum y: i32 = 2"), std::vector<std::string>{"(fixum y:i32 2)"});
    CHECK_EQ(stmts("mut p: ?*u8 = nihil"), std::vector<std::string>{"(mut p:?*u8 nihil)"});
    CHECK_EQ(stmts("x = 5"), std::vector<std::string>{"(set x 5)"});
    CHECK_EQ(stmts("a[0] = 1"), std::vector<std::string>{"(assign (idx a 0) 1)"});
    CHECK_EQ(stmts("p.x = 2"), std::vector<std::string>{"(assign (. p x) 2)"});
    CHECK_EQ(stmts("*p = 3"), std::vector<std::string>{"(assign (deref p) 3)"});
    CHECK_EQ(stmts("p.q[1].r = 4"), std::vector<std::string>{"(assign (. (idx (. p q) 1) r) 4)"});
    CHECK_EQ(stmts("x == 5"), std::vector<std::string>{"(expr (== x 5))"});
    CHECK_EQ(stmts("f(1)"), std::vector<std::string>{"(expr (call f 1))"});
    CHECK_EQ(stmts("rumpe"), std::vector<std::string>{"(rumpe)"});
    CHECK_EQ(stmts("perge"), std::vector<std::string>{"(perge)"});
    CHECK_EQ(stmts("{ x = 1 }"), std::vector<std::string>{"(block (set x 1))"});
    CHECK_EQ(stmts("{ { } }"), std::vector<std::string>{"(block (block))"});
    CHECK_EQ(stmts("audax { f() }"), std::vector<std::string>{"(audax (block (expr (call f))))"});
    CHECK_EQ(stmts(""), std::vector<std::string>{});
}

void testReturn() {
    CHECK_EQ(stmts("redde"), std::vector<std::string>{"(redde)"});
    CHECK_EQ(stmts("redde 1"), std::vector<std::string>{"(redde 1)"});
    CHECK_EQ(stmts("redde a + b"), std::vector<std::string>{"(redde (+ a b))"});
    CHECK_EQ(stmts("redde;"), std::vector<std::string>{"(redde)"});
    CHECK_EQ(stmts("redde; x = 1"), (std::vector<std::string>{"(redde)", "(set x 1)"}));
    // After a bare `redde` the next statement can start on the next line only
    // if `;` or `}` comes first; otherwise the next token is the value.
    CHECK_EQ(stmts("redde\nx"), std::vector<std::string>{"(redde x)"});
    CHECK_EQ(stmts("si a { redde } aliter { redde 1 }"),
             std::vector<std::string>{"(si a (block (redde)) (block (redde 1)))"});
}

void testExit() {
    CHECK_EQ(stmts("exitus libera(p)"), std::vector<std::string>{"(exitus libera p)"});
    CHECK_EQ(stmts("exitus f(a, b)"), std::vector<std::string>{"(exitus f a b)"});
    CHECK_EQ(stmts("exitus a.close()"), std::vector<std::string>{"(exitus (. a close))"});

    // The statement keeps {callee, args} apart.
    const Program program = parse("opus f() { exitus g(1, 2) }");
    const auto& body = *cast<FuncDecl>(*program.items[0]).body;
    const auto& exitStmt = cast<ExitStmt>(*body.stmts[0]);
    CHECK(cast<IdentExpr>(*exitStmt.callee).name == "g");
    CHECK(exitStmt.args.size() == 2);

    CHECK(stmtFailsAt("exitus x", 8, "must be followed by a call"));
    CHECK(stmtFailsAt("exitus f(x) + 1", 8, "must be followed by a call"));
    CHECK(stmtFailsAt("exitus cofre(u8, 1)", 8, "must be followed by a call"));
    CHECK(stmtFailsAt("exitus f(x)?", 8, "must be followed by a call"));
}

void testControlFlow() {
    CHECK_EQ(stmts("si a { x = 1 }"), std::vector<std::string>{"(si a (block (set x 1)))"});
    CHECK_EQ(stmts("si a { x = 1 } aliter { x = 2 }"),
             std::vector<std::string>{"(si a (block (set x 1)) (block (set x 2)))"});
    CHECK_EQ(stmts("si a { x = 1 } aliter si b { x = 2 } aliter { x = 3 }"),
             std::vector<std::string>{"(si a (block (set x 1)) (si b (block (set x 2)) (block (set x 3))))"});
    CHECK_EQ(stmts("si a { } aliter si b { }"), std::vector<std::string>{"(si a (block) (si b (block)))"});
    CHECK_EQ(stmts("si a && b { }"), std::vector<std::string>{"(si (&& a b) (block))"});
    CHECK_EQ(stmts("dum i < 3 { i = i + 1 }"),
             std::vector<std::string>{"(dum (< i 3) (block (set i (+ i 1))))"});
    CHECK_EQ(stmts("per i in 0..10 { f(i) }"),
             std::vector<std::string>{"(per i (.. 0 10) (block (expr (call f i))))"});
    CHECK_EQ(stmts("per i in 0..=n + 1 { }"), std::vector<std::string>{"(per i (..= 0 (+ n 1)) (block))"});
    CHECK_EQ(stmts("per x in items { }"), std::vector<std::string>{"(per x items (block))"});
    CHECK_EQ(stmts("per i in a.len..b { }"), std::vector<std::string>{"(per i (.. (. a len) b) (block))"});
    CHECK_EQ(stmts("si a { } x = 1"), (std::vector<std::string>{"(si a (block))", "(set x 1)"}));

    CHECK(stmtFailsAt("si { }", 4, "expected an expression"));
    CHECK(stmtFailsAt("si a x = 1", 6, "to start the 'si' body"));
    CHECK(stmtFailsAt("dum a", 6, "to start the 'dum' body"));
    CHECK(stmtFailsAt("per in 0..3 { }", 5, "loop variable"));
    CHECK(stmtFailsAt("per i 0..3 { }", 7, "expected 'in'"));
    CHECK(stmtFailsAt("aliter { }", 1, "without a matching 'si'"));
    CHECK(stmtFailsAt("si a { } aliter { } aliter { }", 21, "without a matching 'si'"));
    CHECK(stmtFailsAt("si a { } aliter x", 17, "to start the 'aliter' body"));
    // A range is only valid as the iterable of `per`.
    CHECK(stmtFailsAt("x = 0..3", 6, "found '..'"));
}

void testTerminators() {
    CHECK_EQ(stmts("x = 1; y = 2"), (std::vector<std::string>{"(set x 1)", "(set y 2)"}));
    CHECK_EQ(stmts("x = 1 y = 2"), (std::vector<std::string>{"(set x 1)", "(set y 2)"}));
    CHECK_EQ(stmts(";;; x = 1 ;"), std::vector<std::string>{"(set x 1)"});
    CHECK_EQ(stmts("x = 1\ny = 2\n"), (std::vector<std::string>{"(set x 1)", "(set y 2)"}));

    // Newlines are not significant (SPEC.md section 2.2): a line that starts
    // with an operator continues the previous expression unless `;` ends it.
    CHECK_EQ(stmts("x = a\n-b"), std::vector<std::string>{"(set x (- a b))"});
    CHECK_EQ(stmts("x = a;\n-b"), (std::vector<std::string>{"(set x a)", "(expr (neg b))"}));
    CHECK_EQ(stmts("x = f\n(1)"), std::vector<std::string>{"(set x (call f 1))"});
}

void testDeclarations() {
    CHECK_EQ(items("opus suma(a: i32, b: i32) -> i32 { redde a + b }"),
             std::vector<std::string>{"(opus suma (a:i32 b:i32) -> i32 (block (redde (+ a b))))"});
    CHECK_EQ(items("opus f() { }"), std::vector<std::string>{"(opus f () (block))"});
    CHECK_EQ(items("opus f(a: i32,) { }"), std::vector<std::string>{"(opus f (a:i32) (block))"});
    CHECK_EQ(items("opus f(p: ?*u8, s: []str) -> *i32 { }"),
             std::vector<std::string>{"(opus f (p:?*u8 s:[]str) -> *i32 (block))"});
    CHECK_EQ(items("palam opus g() -> str { redde \"x\" }"),
             std::vector<std::string>{"(pub opus g () -> str (block (redde \"x\")))"});
    CHECK_EQ(items("foris opus abs(x: i32) -> i32"), std::vector<std::string>{"(foris opus abs (x:i32) -> i32)"});
    CHECK_EQ(items("foris opus quit()"), std::vector<std::string>{"(foris opus quit ())"});
    CHECK_EQ(items("forma Punto { x: f64, y: f64 }"), std::vector<std::string>{"(forma Punto x:f64 y:f64)"});
    CHECK_EQ(items("forma P { x: f64, y: f64, }"), std::vector<std::string>{"(forma P x:f64 y:f64)"});
    CHECK_EQ(items("forma Vacia { }"), std::vector<std::string>{"(forma Vacia)"});
    CHECK_EQ(items("palam forma P { x: i32 }"), std::vector<std::string>{"(pub forma P x:i32)"});
    CHECK_EQ(items("fixum MAX = 10"), std::vector<std::string>{"(fixum MAX 10)"});
    CHECK_EQ(items("palam fixum NOMBRE: str = \"ferrum\""),
             std::vector<std::string>{"(pub fixum NOMBRE:str \"ferrum\")"});
    CHECK_EQ(items("importa \"std/fs\""), std::vector<std::string>{"(importa \"std/fs\")"});
    CHECK_EQ(items("importa \"a\"; importa \"b\"\nopus main() { }"),
             (std::vector<std::string>{"(importa \"a\")", "(importa \"b\")", "(opus main () (block))"}));
    CHECK_EQ(items(""), std::vector<std::string>{});
    CHECK_EQ(items("# nada que ver\n"), std::vector<std::string>{});
    CHECK_EQ(items(";;"), std::vector<std::string>{});

    // Flags on the node.
    const Program program = parse("palam opus a() { } foris opus b() opus c() { }");
    CHECK(program.items.size() == 3);
    const auto& a = cast<FuncDecl>(*program.items[0]);
    const auto& b = cast<FuncDecl>(*program.items[1]);
    const auto& c = cast<FuncDecl>(*program.items[2]);
    CHECK(a.isPublic && !a.isExtern && a.body != nullptr);
    CHECK(!b.isPublic && b.isExtern && b.body == nullptr);
    CHECK(!c.isPublic && !c.isExtern);
    CHECK(c.returnType == nullptr);
}

void testSampleProgram() {
    // The sketch from README.md.
    const std::string source = R"fe(
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
    const std::vector<std::string> expected = {
        "(opus suma (a:i32 b:i32) -> i32 (block (redde (+ a b))))",
        "(opus main () -> i32 (block (fixum buffer (cofre u8 4096)) (exitus libera buffer) (mut intentos 0) "
        "(dum (< intentos 3) (block (set intentos (+ intentos 1)))) "
        "(si (> (call suma intentos 1) 3) (block (redde 0)) (block (redde 1)))))",
    };
    CHECK_EQ(items(source), expected);
}

void testLocations() {
    const Program program = parse("opus main() {\n  x = 1 + 2\n  redde\n}\npalam forma P { a: i32 }");
    const auto& main = cast<FuncDecl>(*program.items[0]);
    CHECK(main.loc.line == 1 && main.loc.column == 1);
    CHECK(main.body->loc.line == 1 && main.body->loc.column == 13);

    const auto& assign = cast<AssignOrDeclStmt>(*main.body->stmts[0]);
    CHECK(assign.loc.line == 2 && assign.loc.column == 3);
    // A binary expression is located at its leftmost token.
    CHECK(assign.value->loc.line == 2 && assign.value->loc.column == 7);

    const auto& ret = cast<ReturnStmt>(*main.body->stmts[1]);
    CHECK(ret.loc.line == 3 && ret.loc.column == 3);

    // A `palam` declaration starts at the `palam`; fields have their own position.
    const auto& point = cast<StructDecl>(*program.items[1]);
    CHECK(point.loc.line == 5 && point.loc.column == 1);
    CHECK(point.fields[0].loc.line == 5 && point.fields[0].loc.column == 17);

    const Program params = parse("opus f(alfa: i32, beta: u8) { }");
    const auto& f = cast<FuncDecl>(*params.items[0]);
    CHECK(f.params[0].loc.column == 8 && f.params[1].loc.column == 19);
}

void testReservedSyntax() {
    CHECK(failsAt("genus Color { }", 1, 1, "Phase 5"));
    CHECK(failsAt("discerne x { }", 1, 1, "Phase 6"));
    CHECK(failsAt("opus f() { discerne x { } }", 1, 12, "Phase 6"));
    CHECK(failsAt("opus f() { genus }", 1, 12, "Phase 5"));
    CHECK(failsAt("fixum v: (i32, i32) = 0", 1, 10, "tuple types are reserved"));
    CHECK(failsAt("opus f(p: (i32, i32)) { }", 1, 11, "tuple types are reserved"));
    CHECK(failsAt("fixum v = (1, 2)", 1, 13, "tuple expressions are reserved"));
    CHECK(failsAt("fixum v = genus", 1, 11, "Phase 5"));
    CHECK(failsAt("fixum v = discerne", 1, 11, "Phase 6"));
    CHECK(failsAt("a.1", 1, 1, "expected a declaration"));
    CHECK(exprFailsAt("a.1", 3, "expected a member name"));
}

void testDeclarationErrors() {
    CHECK(failsAt("x = 5", 1, 1, "expected a declaration"));
    CHECK(failsAt("mut x = 5", 1, 1, "'mut' is only allowed inside functions"));
    CHECK(failsAt("palam mut x = 1", 1, 7, "after 'palam'"));
    CHECK(failsAt("palam importa \"a\"", 1, 7, "after 'palam'"));
    CHECK(failsAt("foris opus f() { }", 1, 16, "cannot have a body"));
    CHECK(failsAt("foris f()", 1, 7, "expected 'opus' after 'foris'"));
    CHECK(failsAt("importa x", 1, 9, "string literal"));
    CHECK(failsAt("opus f() { } }", 1, 14, "expected a declaration"));
    CHECK(failsAt("opus (a) { }", 1, 6, "function name"));
    CHECK(failsAt("opus f { }", 1, 8, "expected '(' after the function name"));
    CHECK(failsAt("opus f( { }", 1, 9, "parameter name"));
    CHECK(failsAt("opus f(a i32) { }", 1, 10, "expected ':'"));
    CHECK(failsAt("opus f(a: ) { }", 1, 11, "expected a type"));
    CHECK(failsAt("opus f(a: i32 b: i32) { }", 1, 15, "expected ')'"));
    CHECK(failsAt("opus f() i32 { }", 1, 10, "'-> T'"));
    CHECK(failsAt("opus f() -> { }", 1, 13, "expected a type"));
    CHECK(failsAt("opus f()", 1, 9, "expected '->' or '{'"));
    CHECK(failsAt("forma { }", 1, 7, "type name"));
    CHECK(failsAt("forma P { x: i32 y: i32 }", 1, 18, "expected ',' or '}'"));
    CHECK(failsAt("forma P { x i32 }", 1, 13, "expected ':'"));
    CHECK(failsAt("fixum x", 1, 8, "needs an initializer"));
    CHECK(failsAt("fixum = 1", 1, 7, "variable name"));

    // Declarations cannot be nested.
    CHECK(stmtFailsAt("opus g() { }", 1, "only allowed at the top level"));
    CHECK(stmtFailsAt("forma P { }", 1, "only allowed at the top level"));
    CHECK(stmtFailsAt("importa \"a\"", 1, "only allowed at the top level"));
    CHECK(stmtFailsAt("palam fixum X = 1", 1, "only allowed at the top level"));
}

void testStatementErrors() {
    CHECK(failsAt("opus f() { mut x }", 1, 18, "needs an initializer"));
    CHECK(stmtFailsAt("mut = 1", 5, "variable name after 'mut'"));
    CHECK(stmtFailsAt("1 = 2", 3, "invalid assignment target"));
    CHECK(stmtFailsAt("a + b = 3", 7, "invalid assignment target"));
    CHECK(stmtFailsAt("f() = 1", 5, "invalid assignment target"));
    CHECK(stmtFailsAt("(x) = 1", 5, "invalid assignment target"));
    CHECK(stmtFailsAt("x = ", 5, "expected an expression, found '}'"));
    CHECK(stmtFailsAt("x = = 5", 5, "expected an expression, found '='"));
    CHECK(stmtFailsAt(")", 1, "expected an expression, found ')'"));
    CHECK(stmtFailsAt("f(a", 4, "to close the argument list"));
    CHECK(stmtFailsAt("a[1", 4, "to close the index"));
    CHECK(stmtFailsAt("(a + b", 7, "to close the parenthesized expression"));
    CHECK(stmtFailsAt("a.", 3, "member name"));
    CHECK(stmtFailsAt("x ut", 5, "expected a type"));

    CHECK(failsAt("opus f() {", 1, 11, "expected '}' to close the block opened at 1:10"));
    CHECK(failsAt("opus f() {\n  x = 1\n", 3, 1, "expected '}' to close the block opened at 1:10"));
    CHECK(failsAt("opus f() { si a { }", 1, 20, "expected '}' to close the block opened at 1:10"));
}

void testLexErrorsPropagate() {
    try {
        parse("opus f() { x = @ }");
        CHECK(false);
    } catch (const LexError& e) {
        CHECK(e.line() == 1 && e.column() == 16);
    }

    // The file is lexed completely first: a lexical error later in the file
    // is reported even if a syntax error comes earlier.
    try {
        parse("x = = 5\n@");
        CHECK(false);
    } catch (const LexError& e) {
        CHECK(e.line() == 2 && e.column() == 1);
    } catch (const ParseError&) {
        CHECK(false);
    }
}

void testNestingLimit() {
    const auto nested = [](std::size_t depth) {
        return "fixum v = " + std::string(depth, '(') + "a" + std::string(depth, ')');
    };
    CHECK_EQ(expr(std::string(100, '(') + "a" + std::string(100, ')')), "a");
    CHECK(failsAt(nested(300), 1, 11 + 256, "nesting is too deep"));

    CHECK_EQ(items("fixum v = " + std::string(100, '-') + "a").size(), std::size_t{1});
    CHECK(failsAt("fixum v = " + std::string(300, '-') + "a", 1, 11 + 255, "nesting is too deep"));

    CHECK_EQ(type(std::string(100, '*') + "u8").size(), std::size_t{102});
    CHECK(failsAt("fixum v: " + std::string(300, '*') + "u8 = 0", 1, 10 + 256, "nesting is too deep"));

    CHECK(failsAt("opus f() " + std::string(300, '{') + std::string(300, '}'), 1, 10 + 256, "nesting is too deep"));

    // Long chains that do not recurse in the parser are fine.
    std::string chain = "si a { }";
    for (int i = 0; i < 1000; ++i) {
        chain += " aliter si a { }";
    }
    CHECK_EQ(stmts(chain).size(), std::size_t{1});

    std::string sum = "a";
    for (int i = 0; i < 2000; ++i) {
        sum += " + a";
    }
    CHECK_EQ(parse("fixum v = " + sum).items.size(), std::size_t{1});
}

void testTokenInput() {
    // The parser accepts a token vector without the final Eof...
    std::vector<Token> tokens = Lexer("opus f() { }").tokenize();
    tokens.pop_back();
    Parser withoutEof(std::move(tokens));
    CHECK(withoutEof.parseProgram().items.size() == 1);

    // ...and an empty one.
    Parser empty({});
    CHECK(empty.parseProgram().items.empty());
}

}  // namespace

int main() {
    testPrecedence();
    testUnaryAndCast();
    testComparisons();
    testPostfix();
    testLiterals();
    testMemoryExpressions();
    testTypes();
    testSimpleStatements();
    testReturn();
    testExit();
    testControlFlow();
    testTerminators();
    testDeclarations();
    testSampleProgram();
    testLocations();
    testReservedSyntax();
    testDeclarationErrors();
    testStatementErrors();
    testLexErrorsPropagate();
    testNestingLimit();
    testTokenInput();

    if (failures != 0) {
        std::fprintf(stderr, "test_parser: %d check(s) failed\n", failures);
        return 1;
    }
    std::printf("test_parser: all checks passed\n");
    return 0;
}
