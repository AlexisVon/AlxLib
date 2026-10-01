/*****************************************************************/ /**
 * \file   varsolid_py.h
 * \brief  Python binding — direct binary ↔ Python object serialization
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_PYTHON_VARSOLID_PY_H_
#define _ALEXIS_PYTHON_VARSOLID_PY_H_

#include "avarsolid.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace alx {
    namespace varsolid_py {

        py::object to_object(const bytes_view& _bytes);
        py::object to_value(const bytes_view& _bytes);
        py::object get_value(const bytes_view& _bytes, const std::list<std::string>& _path,
                             const py::object& _def = py::none());

        bytes to_bytes(py::dict _obj);
    }
}

#endif
