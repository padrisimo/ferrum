#include "ast/AST.h"

namespace ferrum {

const char* unaryOpSpelling(UnaryOp op) {
    switch (op) {
    case UnaryOp::Neg:
        return "-";
    case UnaryOp::Not:
        return "!";
    case UnaryOp::BitNot:
        return "~";
    case UnaryOp::Deref:
        return "*";
    case UnaryOp::AddrOf:
        return "&";
    }
    return "?";
}

const char* binaryOpSpelling(BinaryOp op) {
    switch (op) {
    case BinaryOp::Add:
        return "+";
    case BinaryOp::Sub:
        return "-";
    case BinaryOp::Mul:
        return "*";
    case BinaryOp::Div:
        return "/";
    case BinaryOp::Rem:
        return "%";
    case BinaryOp::Shl:
        return "<<";
    case BinaryOp::Shr:
        return ">>";
    case BinaryOp::BitAnd:
        return "&";
    case BinaryOp::BitOr:
        return "|";
    case BinaryOp::BitXor:
        return "^";
    case BinaryOp::Eq:
        return "==";
    case BinaryOp::Ne:
        return "!=";
    case BinaryOp::Lt:
        return "<";
    case BinaryOp::Le:
        return "<=";
    case BinaryOp::Gt:
        return ">";
    case BinaryOp::Ge:
        return ">=";
    case BinaryOp::And:
        return "&&";
    case BinaryOp::Or:
        return "||";
    }
    return "?";
}

const char* builtinName(Builtin which) {
    switch (which) {
    case Builtin::Libera:
        return "libera";
    }
    return "?";
}

}  // namespace ferrum
