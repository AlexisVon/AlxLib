#pragma once

#include "global.h"

namespace filesys {
    /// Set the directory "/" maps to; empty keeps the virtual root (drives / common dirs)
    bool set_root(const std::string& _path, std::string& _error);
    alx::http::api* new_api();
    alx::http::reply root(const alx::http::request& _req);
    alx::http::reply list(const alx::http::request& _req);
    alx::http::reply read(const alx::http::request& _req);
    alx::http::reply upload(const alx::http::request& _req);
    alx::http::reply remove(const alx::http::request& _req);
}
