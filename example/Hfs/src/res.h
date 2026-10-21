#pragma once

#include "abytes.h"
#include <string>

namespace res {
    /// Resolve the resource root once; argv0 is the executable path, possibly relative
    void init(const char* _argv0);
    /// Load one resource by key, e.g. "app.css"; false when it is absent or unreadable
    bool load(const std::string& _key, alx::bytes& _out);
}
