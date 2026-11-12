// Placeholder entry point. The real CLI (--dump-tokens, --dump-ast) replaces
// this file when the printer and CLI land (Phase 1, step F).

#include <cstdio>
#include <cstring>

#include "Version.h"

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
        std::printf("ferrumc %s\n", ferrum::version());
        return 0;
    }

    std::fprintf(stderr, "ferrumc %s: not implemented yet (Phase 1 in progress)\n",
                 ferrum::version());
    return 1;
}
