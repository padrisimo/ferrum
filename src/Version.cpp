#include "Version.h"

#ifndef FERRUM_VERSION
#error "FERRUM_VERSION must be defined by the build system"
#endif

namespace ferrum {

const char* version() {
    return FERRUM_VERSION;
}

}  // namespace ferrum
