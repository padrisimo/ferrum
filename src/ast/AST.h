#ifndef FERRUM_AST_AST_H
#define FERRUM_AST_AST_H

// The abstract syntax tree produced by the parser (SPEC.md, Appendix A).
//
// Every node owns its children through std::unique_ptr and is non-copyable.
// Nodes are tagged with a `kind` instead of relying on RTTI; use isa<T>(),
// cast<T>() and dynCast<T>() to go from a base reference to a concrete node.
// Constructors take every field, so a node is never half-initialized.

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ferrum {

/// 1-based position in the source file. {0, 0} means "unknown".
struct SourceLoc {
    std::uint32_t line = 0;
    std::uint32_t column = 0;
};

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

enum class TypeKind : std::uint8_t { Named, Pointer, Optional, Slice };

struct TypeNode {
    const TypeKind kind;
    SourceLoc loc;

    TypeNode(const TypeNode&) = delete;
    TypeNode& operator=(const TypeNode&) = delete;
    virtual ~TypeNode() = default;

protected:
    TypeNode(TypeKind kind_, SourceLoc at) : kind(kind_), loc(at) {}
};
using TypePtr = std::unique_ptr<TypeNode>;

/// A primitive type name (i32, str, vacuum, ...) or a user type (forma) name.
struct NamedType final : TypeNode {
    static constexpr TypeKind kKind = TypeKind::Named;
    NamedType(SourceLoc at, std::string name_) : TypeNode(kKind, at), name(std::move(name_)) {}
    std::string name;
};

/// *T
struct PointerType final : TypeNode {
    static constexpr TypeKind kKind = TypeKind::Pointer;
    PointerType(SourceLoc at, TypePtr pointee_) : TypeNode(kKind, at), pointee(std::move(pointee_)) {}
    TypePtr pointee;
};

/// ?T
struct OptionalType final : TypeNode {
    static constexpr TypeKind kKind = TypeKind::Optional;
    OptionalType(SourceLoc at, TypePtr inner_) : TypeNode(kKind, at), inner(std::move(inner_)) {}
    TypePtr inner;
};

/// []T
struct SliceType final : TypeNode {
    static constexpr TypeKind kKind = TypeKind::Slice;
    SliceType(SourceLoc at, TypePtr element_) : TypeNode(kKind, at), element(std::move(element_)) {}
    TypePtr element;
};

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

enum class ExprKind : std::uint8_t {
    IntLit,
    FloatLit,
    StrLit,
    BoolLit,
    NihilLit,
    Ident,
    Builtin,
    Unary,
    Binary,
    Cast,
    Call,
    Index,
    Member,
    Propagate,
    Alloc,
    Range,
};

enum class UnaryOp : std::uint8_t {
    Neg,     // -x
    Not,     // !x
    BitNot,  // ~x
    Deref,   // *p
    AddrOf,  // &x
};

enum class BinaryOp : std::uint8_t {
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    Shl,
    Shr,
    BitAnd,
    BitOr,
    BitXor,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,  // &&
    Or,   // ||
};

/// Functions built into the language. They are keywords, so they cannot be
/// shadowed or passed around; they appear as the callee of a CallExpr.
enum class Builtin : std::uint8_t {
    Libera,  // libera(p)
};

struct Expr {
    const ExprKind kind;
    SourceLoc loc;

    Expr(const Expr&) = delete;
    Expr& operator=(const Expr&) = delete;
    virtual ~Expr() = default;

protected:
    Expr(ExprKind kind_, SourceLoc at) : kind(kind_), loc(at) {}
};
using ExprPtr = std::unique_ptr<Expr>;

/// An integer literal. Literals carry no sign: "-5" is a Neg applied to 5.
struct IntLitExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::IntLit;
    IntLitExpr(SourceLoc at, std::uint64_t value_) : Expr(kKind, at), value(value_) {}
    std::uint64_t value;
};

struct FloatLitExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::FloatLit;
    FloatLitExpr(SourceLoc at, double value_) : Expr(kKind, at), value(value_) {}
    double value;
};

/// A string literal with its escapes already decoded.
struct StrLitExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::StrLit;
    StrLitExpr(SourceLoc at, std::string value_) : Expr(kKind, at), value(std::move(value_)) {}
    std::string value;
};

/// verum / falsum
struct BoolLitExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::BoolLit;
    BoolLitExpr(SourceLoc at, bool value_) : Expr(kKind, at), value(value_) {}
    bool value;
};

/// nihil
struct NihilLitExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::NihilLit;
    explicit NihilLitExpr(SourceLoc at) : Expr(kKind, at) {}
};

struct IdentExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Ident;
    IdentExpr(SourceLoc at, std::string name_) : Expr(kKind, at), name(std::move(name_)) {}
    std::string name;
};

/// A reference to a built-in function, e.g. the `libera` in `libera(p)`.
struct BuiltinExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Builtin;
    BuiltinExpr(SourceLoc at, Builtin which_) : Expr(kKind, at), which(which_) {}
    Builtin which;
};

struct UnaryExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Unary;
    UnaryExpr(SourceLoc at, UnaryOp op_, ExprPtr operand_)
        : Expr(kKind, at), op(op_), operand(std::move(operand_)) {}
    UnaryOp op;
    ExprPtr operand;
};

struct BinaryExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Binary;
    BinaryExpr(SourceLoc at, BinaryOp op_, ExprPtr lhs_, ExprPtr rhs_)
        : Expr(kKind, at), op(op_), lhs(std::move(lhs_)), rhs(std::move(rhs_)) {}
    BinaryOp op;
    ExprPtr lhs;
    ExprPtr rhs;
};

/// `operand ut target`
struct CastExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Cast;
    CastExpr(SourceLoc at, ExprPtr operand_, TypePtr target_)
        : Expr(kKind, at), operand(std::move(operand_)), target(std::move(target_)) {}
    ExprPtr operand;
    TypePtr target;
};

/// `callee(args...)`. The callee is an arbitrary expression (usually an
/// IdentExpr or MemberExpr) or a BuiltinExpr.
struct CallExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Call;
    CallExpr(SourceLoc at, ExprPtr callee_, std::vector<ExprPtr> args_)
        : Expr(kKind, at), callee(std::move(callee_)), args(std::move(args_)) {}
    ExprPtr callee;
    std::vector<ExprPtr> args;
};

/// `base[index]`
struct IndexExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Index;
    IndexExpr(SourceLoc at, ExprPtr base_, ExprPtr index_)
        : Expr(kKind, at), base(std::move(base_)), index(std::move(index_)) {}
    ExprPtr base;
    ExprPtr index;
};

/// `base.member`
struct MemberExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Member;
    MemberExpr(SourceLoc at, ExprPtr base_, std::string member_)
        : Expr(kKind, at), base(std::move(base_)), member(std::move(member_)) {}
    ExprPtr base;
    std::string member;
};

/// `operand?`: the optional-propagation operator (Phase 2).
struct PropagateExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Propagate;
    PropagateExpr(SourceLoc at, ExprPtr operand_) : Expr(kKind, at), operand(std::move(operand_)) {}
    ExprPtr operand;
};

/// `cofre(elementType, count)`: typed heap allocation, yields *elementType.
struct AllocExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Alloc;
    AllocExpr(SourceLoc at, TypePtr elementType_, ExprPtr count_)
        : Expr(kKind, at), elementType(std::move(elementType_)), count(std::move(count_)) {}
    TypePtr elementType;
    ExprPtr count;
};

/// `start..end` or `start..=end`. Only valid as the iterable of a `per` loop.
struct RangeExpr final : Expr {
    static constexpr ExprKind kKind = ExprKind::Range;
    RangeExpr(SourceLoc at, ExprPtr start_, ExprPtr end_, bool inclusive_)
        : Expr(kKind, at), start(std::move(start_)), end(std::move(end_)), inclusive(inclusive_) {}
    ExprPtr start;
    ExprPtr end;
    bool inclusive;
};

// ---------------------------------------------------------------------------
// Statements and declarations
// ---------------------------------------------------------------------------

enum class StmtKind : std::uint8_t {
    VarDecl,
    AssignOrDecl,
    Assign,
    Return,
    Exit,
    Break,
    Continue,
    If,
    While,
    For,
    Block,
    Audax,
    Expr,
    FuncDecl,
    StructDecl,
    ImportDecl,
};

struct Stmt {
    const StmtKind kind;
    SourceLoc loc;

    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;
    virtual ~Stmt() = default;

protected:
    Stmt(StmtKind kind_, SourceLoc at) : kind(kind_), loc(at) {}
};
using StmtPtr = std::unique_ptr<Stmt>;

/// `{ stmts... }`
struct BlockStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::Block;
    BlockStmt(SourceLoc at, std::vector<StmtPtr> stmts_) : Stmt(kKind, at), stmts(std::move(stmts_)) {}
    std::vector<StmtPtr> stmts;
};

enum class VarKind : std::uint8_t {
    Mut,    // mut x = e
    Fixum,  // fixum x = e
};

/// `mut name [: type] = init` or `fixum name [: type] = init`. At the top
/// level of a file a `fixum` declaration may be marked `palam`.
struct VarDecl final : Stmt {
    static constexpr StmtKind kKind = StmtKind::VarDecl;
    VarDecl(SourceLoc at, VarKind varKind_, std::string name_, TypePtr type_, ExprPtr init_,
            bool isPublic_ = false)
        : Stmt(kKind, at),
          varKind(varKind_),
          name(std::move(name_)),
          type(std::move(type_)),
          init(std::move(init_)),
          isPublic(isPublic_) {}
    VarKind varKind;
    std::string name;
    TypePtr type;  // null when inferred
    ExprPtr init;
    bool isPublic;
};

/// `name = value` with a bare identifier on the left. Semantic analysis
/// decides whether this assigns an existing binding or declares a new
/// immutable one (SPEC.md section 5.1).
struct AssignOrDeclStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::AssignOrDecl;
    AssignOrDeclStmt(SourceLoc at, std::string name_, ExprPtr value_)
        : Stmt(kKind, at), name(std::move(name_)), value(std::move(value_)) {}
    std::string name;
    ExprPtr value;
};

/// `target = value` where target is an index, member or dereference.
struct AssignStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::Assign;
    AssignStmt(SourceLoc at, ExprPtr target_, ExprPtr value_)
        : Stmt(kKind, at), target(std::move(target_)), value(std::move(value_)) {}
    ExprPtr target;
    ExprPtr value;
};

/// `redde [value]`
struct ReturnStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::Return;
    ReturnStmt(SourceLoc at, ExprPtr value_) : Stmt(kKind, at), value(std::move(value_)) {}
    ExprPtr value;  // null for a bare `redde`
};

/// `exitus callee(args...)`: a call deferred to the end of the enclosing
/// block. Kept as {callee, args} rather than a CallExpr because the arguments
/// are evaluated at the `exitus` and the call itself happens later.
struct ExitStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::Exit;
    ExitStmt(SourceLoc at, ExprPtr callee_, std::vector<ExprPtr> args_)
        : Stmt(kKind, at), callee(std::move(callee_)), args(std::move(args_)) {}
    ExprPtr callee;
    std::vector<ExprPtr> args;
};

/// `rumpe`
struct BreakStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::Break;
    explicit BreakStmt(SourceLoc at) : Stmt(kKind, at) {}
};

/// `perge`
struct ContinueStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::Continue;
    explicit ContinueStmt(SourceLoc at) : Stmt(kKind, at) {}
};

/// `si cond { ... } [aliter ...]`. `elseBranch` is a BlockStmt for a plain
/// `aliter { ... }`, an IfStmt for `aliter si ...`, or null.
struct IfStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::If;
    IfStmt(SourceLoc at, ExprPtr cond_, std::unique_ptr<BlockStmt> thenBlock_, StmtPtr elseBranch_)
        : Stmt(kKind, at),
          cond(std::move(cond_)),
          thenBlock(std::move(thenBlock_)),
          elseBranch(std::move(elseBranch_)) {}
    ExprPtr cond;
    std::unique_ptr<BlockStmt> thenBlock;
    StmtPtr elseBranch;
};

/// `dum cond { ... }`
struct WhileStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::While;
    WhileStmt(SourceLoc at, ExprPtr cond_, std::unique_ptr<BlockStmt> body_)
        : Stmt(kKind, at), cond(std::move(cond_)), body(std::move(body_)) {}
    ExprPtr cond;
    std::unique_ptr<BlockStmt> body;
};

/// `per var in iterable { ... }`. The iterable may be a RangeExpr.
struct ForStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::For;
    ForStmt(SourceLoc at, std::string var_, ExprPtr iterable_, std::unique_ptr<BlockStmt> body_)
        : Stmt(kKind, at), var(std::move(var_)), iterable(std::move(iterable_)), body(std::move(body_)) {}
    std::string var;
    ExprPtr iterable;
    std::unique_ptr<BlockStmt> body;
};

/// `audax { ... }`: a block where risky operations are allowed.
struct AudaxStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::Audax;
    AudaxStmt(SourceLoc at, std::unique_ptr<BlockStmt> body_) : Stmt(kKind, at), body(std::move(body_)) {}
    std::unique_ptr<BlockStmt> body;
};

/// An expression evaluated for its effect.
struct ExprStmt final : Stmt {
    static constexpr StmtKind kKind = StmtKind::Expr;
    ExprStmt(SourceLoc at, ExprPtr expr_) : Stmt(kKind, at), expr(std::move(expr_)) {}
    ExprPtr expr;
};

struct Param {
    SourceLoc loc;
    std::string name;
    TypePtr type;
};

/// `[palam] opus name(params) [-> type] { body }`, or, for `foris`, a
/// declaration without a body (`isExtern`, `body` null).
struct FuncDecl final : Stmt {
    static constexpr StmtKind kKind = StmtKind::FuncDecl;
    FuncDecl(SourceLoc at, bool isPublic_, bool isExtern_, std::string name_, std::vector<Param> params_,
             TypePtr returnType_, std::unique_ptr<BlockStmt> body_)
        : Stmt(kKind, at),
          isPublic(isPublic_),
          isExtern(isExtern_),
          name(std::move(name_)),
          params(std::move(params_)),
          returnType(std::move(returnType_)),
          body(std::move(body_)) {}
    bool isPublic;
    bool isExtern;
    std::string name;
    std::vector<Param> params;
    TypePtr returnType;  // null means vacuum
    std::unique_ptr<BlockStmt> body;
};

struct Field {
    SourceLoc loc;
    std::string name;
    TypePtr type;
};

/// `[palam] forma Name { field: type, ... }`
struct StructDecl final : Stmt {
    static constexpr StmtKind kKind = StmtKind::StructDecl;
    StructDecl(SourceLoc at, bool isPublic_, std::string name_, std::vector<Field> fields_)
        : Stmt(kKind, at), isPublic(isPublic_), name(std::move(name_)), fields(std::move(fields_)) {}
    bool isPublic;
    std::string name;
    std::vector<Field> fields;
};

/// `importa "path"`
struct ImportDecl final : Stmt {
    static constexpr StmtKind kKind = StmtKind::ImportDecl;
    ImportDecl(SourceLoc at, std::string path_) : Stmt(kKind, at), path(std::move(path_)) {}
    std::string path;  // decoded
};

/// A whole source file: imports and top-level declarations (ImportDecl,
/// FuncDecl, StructDecl, VarDecl), in source order.
struct Program {
    std::vector<StmtPtr> items;
};

// ---------------------------------------------------------------------------
// Casting helpers (kind-based, no RTTI)
// ---------------------------------------------------------------------------

/// True if `node` is a T. T is a concrete node type, e.g. isa<BinaryExpr>(e).
template <typename T, typename Base>
bool isa(const Base& node) {
    return node.kind == T::kKind;
}

/// Downcasts to T. The node must be a T (checked by assert in Debug builds).
template <typename T, typename Base>
std::conditional_t<std::is_const_v<Base>, const T&, T&> cast(Base& node) {
    assert(isa<T>(node) && "cast<T>() applied to a node of a different kind");
    return static_cast<std::conditional_t<std::is_const_v<Base>, const T&, T&>>(node);
}

/// Downcasts to T, or returns null if `node` is null or not a T.
template <typename T, typename Base>
std::conditional_t<std::is_const_v<Base>, const T*, T*> dynCast(Base* node) {
    if (node == nullptr || !isa<T>(*node)) {
        return nullptr;
    }
    return static_cast<std::conditional_t<std::is_const_v<Base>, const T*, T*>>(node);
}

// ---------------------------------------------------------------------------
// Names for printing and diagnostics
// ---------------------------------------------------------------------------

/// Source spelling of an operator, e.g. "-" or "&&".
const char* unaryOpSpelling(UnaryOp op);
const char* binaryOpSpelling(BinaryOp op);

/// Source spelling of a built-in function, e.g. "libera".
const char* builtinName(Builtin which);

}  // namespace ferrum

#endif  // FERRUM_AST_AST_H
