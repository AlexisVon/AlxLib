/*****************************************************************/ /**
 * \file   interface.h
 * \brief  Node.js addon — module interface declarations
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 *********************************************************************/

#ifndef _ALEXIS_NODE_INTERFACE_H_
#define _ALEXIS_NODE_INTERFACE_H_

#include "args_wrap.h"
#include "global_node.h"

#define STRINGIFY(STR) #STR
#define TOSTRING(X) STRINGIFY(X)
#define ALXVAR_VERSION_MAJOR 0
#define ALXVAR_VERSION_MINOR 0
#define ALXVAR_VERSION_PATCH 0
#define ALXVAR_VERSION_BUILD 0
#define ALXVAR_VERSION TOSTRING(ALXVAR_VERSION_MAJOR) "." TOSTRING(ALXVAR_VERSION_MINOR) "." TOSTRING(ALXVAR_VERSION_PATCH) "." TOSTRING(ALXVAR_VERSION_BUILD)

void init(Local<Object> _exports, Local<Value> _module, void* _priv);

void version(args_wrap _args);

void encode(args_wrap _args);

void decode(args_wrap _args);

#endif