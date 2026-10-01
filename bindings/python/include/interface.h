/*****************************************************************/ /**
 * \file   interface.h
 * \brief  Python binding — module interface declarations
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_PYTHON_INTERFACE_H_
#define _ALEXIS_PYTHON_INTERFACE_H_

#include <pybind11/pybind11.h>

namespace py = pybind11;

#define STRINGIFY(STR) #STR
#define TOSTRING(X) STRINGIFY(X)
#define ALXVAR_VERSION_MAJOR 0
#define ALXVAR_VERSION_MINOR 0
#define ALXVAR_VERSION_PATCH 0
#define ALXVAR_VERSION_BUILD 0
#define ALXVAR_VERSION TOSTRING(ALXVAR_VERSION_MAJOR) "." TOSTRING(ALXVAR_VERSION_MINOR) "." TOSTRING(ALXVAR_VERSION_PATCH) "." TOSTRING(ALXVAR_VERSION_BUILD)

std::string version();

py::bytes encode(py::dict _obj);

py::object decode(py::args _args);

#endif
