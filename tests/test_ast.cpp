// AST tests: node construction, ownership, kind-based casting and operator
// names. A plain main(): returns 0 on success, 1 on any failed check.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ast/AST.h"

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

constexpr SourceLoc kLoc{3, 7};

ExprPtr ident(const char* name) {
    return std::make_unique<IdentExpr>(kLoc, name);
}

ExprPtr number(std::uint64_t value) {
    return std::make_unique<IntLitExpr>(kLoc, value);
}

TypePtr named(const char* name) {
    return std::make_unique<NamedType>(kLoc, name);
}

ExprPtr binary(BinaryOp op, ExprPtr lhs, ExprPtr rhs) {
    return std::make_unique<BinaryExpr>(kLoc, op, std::move(lhs), std::move(rhs));
}

std::unique_ptr<BlockStmt> block(std::vector<StmtPtr> stmts) {
    return std::make_unique<BlockStmt>(kLoc, std::move(stmts));
}

void testNodesAreNotCopyable() {
    static_assert(!std::is_copy_constructible_v<Expr>);
    static_assert(!std::is_copy_constructible_v<BinaryExpr>);
    static_assert(!std::is_copy_constructible_v<Stmt>);
    static_assert(!std::is_copy_constructible_v<IfStmt>);
    static_assert(!std::is_copy_constructible_v<TypeNode>);
    static_assert(std::has_virtual_destructor_v<Expr>);
    static_assert(std::has_virtual_destructor_v<Stmt>);
    static_assert(std::has_virtual_destructor_v<TypeNode>);
}

void testCasting() {
    ExprPtr e = number(42);
    CHECK(e->kind == ExprKind::IntLit);
    CHECK(isa<IntLitExpr>(*e));
    CHECK(!isa<IdentExpr>(*e));
    CHECK(cast<IntLitExpr>(*e).value == 42);
    CHECK(e->loc.line == 3 && e->loc.column == 7);

    // dynCast: right kind, wrong kind, null; const and non-const.
    CHECK(dynCast<IntLitExpr>(e.get()) != nullptr);
    CHECK(dynCast<IdentExpr>(e.get()) == nullptr);
    const Expr* constPtr = e.get();
    CHECK(dynCast<IntLitExpr>(constPtr) != nullptr);
    CHECK(dynCast<IntLitExpr>(constPtr)->value == 42);
    CHECK(dynCast<IntLitExpr>(static_cast<Expr*>(nullptr)) == nullptr);

    // cast<> keeps constness.
    const Expr& constRef = *e;
    static_assert(std::is_same_v<decltype(cast<IntLitExpr>(constRef)), const IntLitExpr&>);
    static_assert(std::is_same_v<decltype(cast<IntLitExpr>(*e)), IntLitExpr&>);
    cast<IntLitExpr>(*e).value = 7;
    CHECK(cast<IntLitExpr>(constRef).value == 7);

    StmtPtr s = std::make_unique<BreakStmt>(kLoc);
    CHECK(isa<BreakStmt>(*s));
    CHECK(!isa<ContinueStmt>(*s));

    TypePtr t = named("i32");
    CHECK(isa<NamedType>(*t));
    CHECK(cast<NamedType>(*t).name == "i32");
}

void testLiterals() {
    CHECK(cast<FloatLitExpr>(*std::make_unique<FloatLitExpr>(kLoc, 2.5)).value == 2.5);
    CHECK(cast<StrLitExpr>(*std::make_unique<StrLitExpr>(kLoc, "hi\n")).value == "hi\n");
    CHECK(cast<BoolLitExpr>(*std::make_unique<BoolLitExpr>(kLoc, true)).value);
    CHECK(std::make_unique<NihilLitExpr>(kLoc)->kind == ExprKind::NihilLit);
    CHECK(number(UINT64_MAX)->kind == ExprKind::IntLit);
}

void testExpressionTree() {
    // a + b * 2
    ExprPtr e = binary(BinaryOp::Add, ident("a"), binary(BinaryOp::Mul, ident("b"), number(2)));

    const auto& add = cast<BinaryExpr>(*e);
    CHECK(add.op == BinaryOp::Add);
    CHECK(cast<IdentExpr>(*add.lhs).name == "a");

    const auto* mul = dynCast<BinaryExpr>(add.rhs.get());
    CHECK(mul != nullptr);
    CHECK(mul->op == BinaryOp::Mul);
    CHECK(cast<IdentExpr>(*mul->lhs).name == "b");
    CHECK(cast<IntLitExpr>(*mul->rhs).value == 2);

    // Other expression forms: *p, x ut i64, e?, a[i], a.b, f(x, y)
    UnaryExpr deref(kLoc, UnaryOp::Deref, ident("p"));
    CHECK(deref.op == UnaryOp::Deref && isa<IdentExpr>(*deref.operand));

    CastExpr cst(kLoc, ident("x"), named("i64"));
    CHECK(cast<NamedType>(*cst.target).name == "i64");

    PropagateExpr prop(kLoc, ident("e"));
    CHECK(isa<IdentExpr>(*prop.operand));

    IndexExpr idx(kLoc, ident("a"), ident("i"));
    CHECK(isa<IdentExpr>(*idx.base) && isa<IdentExpr>(*idx.index));

    MemberExpr mem(kLoc, ident("a"), "b");
    CHECK(mem.member == "b");

    std::vector<ExprPtr> args;
    args.push_back(ident("x"));
    args.push_back(number(1));
    CallExpr call(kLoc, ident("f"), std::move(args));
    CHECK(call.args.size() == 2);
    CHECK(args.empty());  // moved from

    RangeExpr range(kLoc, number(0), number(10), true);
    CHECK(range.inclusive && isa<IntLitExpr>(*range.end));
}

void testTypes() {
    // ?*u8
    TypePtr t = std::make_unique<OptionalType>(kLoc, std::make_unique<PointerType>(kLoc, named("u8")));
    const auto& opt = cast<OptionalType>(*t);
    const auto& ptr = cast<PointerType>(*opt.inner);
    CHECK(cast<NamedType>(*ptr.pointee).name == "u8");

    // []i32
    SliceType slice(kLoc, named("i32"));
    CHECK(cast<NamedType>(*slice.element).name == "i32");
}

void testMemoryNodes() {
    // exitus libera(buffer)
    std::vector<ExprPtr> args;
    args.push_back(ident("buffer"));
    ExitStmt exitStmt(kLoc, std::make_unique<BuiltinExpr>(kLoc, Builtin::Libera), std::move(args));
    CHECK(isa<BuiltinExpr>(*exitStmt.callee));
    CHECK(cast<BuiltinExpr>(*exitStmt.callee).which == Builtin::Libera);
    CHECK(exitStmt.args.size() == 1);
    CHECK(std::string(builtinName(Builtin::Libera)) == "libera");

    // cofre(u8, 4096)
    AllocExpr alloc(kLoc, named("u8"), number(4096));
    CHECK(cast<NamedType>(*alloc.elementType).name == "u8");
    CHECK(cast<IntLitExpr>(*alloc.count).value == 4096);
}

void testStatements() {
    // mut n = 0   /   fixum limite: i32 = 10   /   x = 5   /   p.x = 1
    VarDecl mut(kLoc, VarKind::Mut, "n", nullptr, number(0));
    CHECK(mut.varKind == VarKind::Mut && mut.type == nullptr && !mut.isPublic);
    VarDecl fixum(kLoc, VarKind::Fixum, "limite", named("i32"), number(10), true);
    CHECK(fixum.type != nullptr && fixum.isPublic);
    AssignOrDeclStmt bare(kLoc, "x", number(5));
    CHECK(bare.name == "x");
    AssignStmt assign(kLoc, std::make_unique<MemberExpr>(kLoc, ident("p"), "x"), number(1));
    CHECK(isa<MemberExpr>(*assign.target));

    // redde / redde 1
    ReturnStmt bareReturn(kLoc, nullptr);
    CHECK(bareReturn.value == nullptr);
    ReturnStmt valueReturn(kLoc, number(1));
    CHECK(valueReturn.value != nullptr);

    // si a { } aliter si b { } aliter { }
    StmtPtr elseBlock = block({});
    StmtPtr elseIf = std::make_unique<IfStmt>(kLoc, ident("b"), block({}), std::move(elseBlock));
    IfStmt ifStmt(kLoc, ident("a"), block({}), std::move(elseIf));
    const auto* second = dynCast<IfStmt>(ifStmt.elseBranch.get());
    CHECK(second != nullptr);
    CHECK(second != nullptr && isa<BlockStmt>(*second->elseBranch));
    CHECK(IfStmt(kLoc, ident("a"), block({}), nullptr).elseBranch == nullptr);

    // dum / per i in 0..=9 / audax
    WhileStmt whileStmt(kLoc, ident("c"), block({}));
    CHECK(whileStmt.body != nullptr);
    ForStmt forStmt(kLoc, "i", std::make_unique<RangeExpr>(kLoc, number(0), number(9), true), block({}));
    CHECK(forStmt.var == "i" && isa<RangeExpr>(*forStmt.iterable));
    AudaxStmt audax(kLoc, block({}));
    CHECK(audax.body != nullptr);

    // expression statement
    ExprStmt exprStmt(kLoc, std::make_unique<CallExpr>(kLoc, ident("f"), std::vector<ExprPtr>{}));
    CHECK(isa<CallExpr>(*exprStmt.expr));
}

void testProgram() {
    // opus suma(a: i32, b: i32) -> i32 { redde a + b }
    std::vector<Param> params;
    params.push_back(Param{kLoc, "a", named("i32")});
    params.push_back(Param{kLoc, "b", named("i32")});
    std::vector<StmtPtr> body;
    body.push_back(std::make_unique<ReturnStmt>(kLoc, binary(BinaryOp::Add, ident("a"), ident("b"))));

    Program program;
    program.items.push_back(std::make_unique<ImportDecl>(kLoc, "std/fs"));
    program.items.push_back(std::make_unique<FuncDecl>(kLoc, true, false, "suma", std::move(params),
                                                       named("i32"), block(std::move(body))));

    // foris opus abs(x: i32) -> i32      (no body)
    std::vector<Param> externParams;
    externParams.push_back(Param{kLoc, "x", named("i32")});
    program.items.push_back(std::make_unique<FuncDecl>(kLoc, false, true, "abs", std::move(externParams),
                                                       named("i32"), nullptr));

    // palam forma Punto { x: f64, y: f64 }
    std::vector<Field> fields;
    fields.push_back(Field{kLoc, "x", named("f64")});
    fields.push_back(Field{kLoc, "y", named("f64")});
    program.items.push_back(std::make_unique<StructDecl>(kLoc, true, "Punto", std::move(fields)));

    // fixum MAX = 10
    program.items.push_back(std::make_unique<VarDecl>(kLoc, VarKind::Fixum, "MAX", nullptr, number(10)));

    CHECK(program.items.size() == 5);
    CHECK(isa<ImportDecl>(*program.items[0]));
    CHECK(cast<ImportDecl>(*program.items[0]).path == "std/fs");

    const auto& suma = cast<FuncDecl>(*program.items[1]);
    CHECK(suma.isPublic && !suma.isExtern && suma.name == "suma");
    CHECK(suma.params.size() == 2 && suma.params[1].name == "b");
    CHECK(suma.returnType != nullptr && suma.body != nullptr);
    CHECK(suma.body->stmts.size() == 1);
    const auto& ret = cast<ReturnStmt>(*suma.body->stmts[0]);
    CHECK(isa<BinaryExpr>(*ret.value));

    const auto& ext = cast<FuncDecl>(*program.items[2]);
    CHECK(ext.isExtern && ext.body == nullptr);

    const auto& punto = cast<StructDecl>(*program.items[3]);
    CHECK(punto.isPublic && punto.fields.size() == 2 && punto.fields[0].name == "x");

    CHECK(isa<VarDecl>(*program.items[4]));

    // A function with no declared return type returns vacuum.
    FuncDecl noReturn(kLoc, false, false, "f", {}, nullptr, block({}));
    CHECK(noReturn.returnType == nullptr);
}

void testOperatorNames() {
    const std::pair<BinaryOp, const char*> binaries[] = {
        {BinaryOp::Add, "+"},   {BinaryOp::Sub, "-"},    {BinaryOp::Mul, "*"},   {BinaryOp::Div, "/"},
        {BinaryOp::Rem, "%"},   {BinaryOp::Shl, "<<"},   {BinaryOp::Shr, ">>"},  {BinaryOp::BitAnd, "&"},
        {BinaryOp::BitOr, "|"}, {BinaryOp::BitXor, "^"}, {BinaryOp::Eq, "=="},   {BinaryOp::Ne, "!="},
        {BinaryOp::Lt, "<"},    {BinaryOp::Le, "<="},    {BinaryOp::Gt, ">"},    {BinaryOp::Ge, ">="},
        {BinaryOp::And, "&&"},  {BinaryOp::Or, "||"},
    };
    for (const auto& [op, text] : binaries) {
        CHECK(std::string(binaryOpSpelling(op)) == text);
    }

    const std::pair<UnaryOp, const char*> unaries[] = {
        {UnaryOp::Neg, "-"}, {UnaryOp::Not, "!"}, {UnaryOp::BitNot, "~"},
        {UnaryOp::Deref, "*"}, {UnaryOp::AddrOf, "&"},
    };
    for (const auto& [op, text] : unaries) {
        CHECK(std::string(unaryOpSpelling(op)) == text);
    }
}

void testDeepTreeIsFreed() {
    // 1 + 1 + 1 + ... left-deep: the whole tree must be released without leaks
    // or crashes (checked by the sanitizers in Debug builds).
    ExprPtr e = number(1);
    for (int i = 0; i < 2000; ++i) {
        e = binary(BinaryOp::Add, std::move(e), number(1));
    }
    CHECK(isa<BinaryExpr>(*e));
}

}  // namespace

int main() {
    testNodesAreNotCopyable();
    testCasting();
    testLiterals();
    testExpressionTree();
    testTypes();
    testMemoryNodes();
    testStatements();
    testProgram();
    testOperatorNames();
    testDeepTreeIsFreed();

    if (failures != 0) {
        std::fprintf(stderr, "test_ast: %d check(s) failed\n", failures);
        return 1;
    }
    std::printf("test_ast: all checks passed\n");
    return 0;
}
