/*****************************************************************/ /**
 * \file   interface.cpp
 * \brief  Python binding — module interface
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#include "interface.h"
#include "aserial.h"
#include "avarsolid.h"
#include "varsolid_py.h"
#include <list>
#include <string>

using namespace alx;
using namespace alx::ser;

std::string version() {
    return "v" ALXVAR_VERSION ", build: " __TIME__ " " __DATE__;
}

py::bytes encode(py::dict _obj) {
    bytes result = varsolid_py::to_bytes(_obj);
    if (result.empty()) {
        if (_obj.size() > 0) {

            throw py::type_error("encode(dict): dict contains unsupported value type");
        }

        return py::bytes("");
    }
    return py::bytes((const char*) result.data(), result.size());
}

py::object decode(py::args _args) {
    if (_args.size() == 0)
        throw py::type_error("decode() missing required argument 'buf' (expected bytes)");

    if (!py::isinstance<py::bytes>(_args[0]))
        throw py::type_error("decode(buf: bytes, *path: str) — buf must be bytes");

    std::string raw = _args[0].cast<std::string>();
    bytes raw_bytes((const BIT8*) raw.data(), raw.size());
    bytes_view bv(raw_bytes);

    if (_args.size() == 1)
        return varsolid_py::to_object(bv);

    std::list<std::string> path;
    for (size_t i = 1; i < _args.size(); i++) {
        if (!py::isinstance<py::str>(_args[i]))
            throw py::type_error("decode(buf: bytes, *path: str) — path keys must be strings");
        path.push_back(_args[i].cast<std::string>());
    }
    return varsolid_py::get_value(bv, path);
}

PYBIND11_MODULE(alxbase, m) {
    m.doc() = "AlxBase — high-performance binary serialization for Python";

    m.def("version", &version, "Return the library version string");
    m.def("encode", &encode, "Encode a Python dict to binary bytes",
          py::arg("obj"));
    m.def("decode", &decode, "Decode binary bytes to a Python dict, "
                             "or extract a value at the given path");
}
