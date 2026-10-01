/*****************************************************************/ /**
 * \file   ascript_enum.h
 * \brief  Script enums — op_enum + tk_enum
 *
 * \author alexis
 *
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#ifndef _ALEXIS_SCRIPT_ENUM_H_
#define _ALEXIS_SCRIPT_ENUM_H_

#include "abase.h"

namespace alx {
    namespace script {

        // Every AST node is a varvec whose first element is an OPTYPE; the walker dispatches on it
        using OPTYPE = uint_8;
        // Opcode values are serialized into compiled .axp products: append new ones, never renumber
        enum op_enum : OPTYPE {
            O_NOP = 0,

            O_PROGRAM,
            O_BLOCK,

            O_VAR,
            O_LOAD,
            // [O_ILOAD, expr] -- expr evaluates to the name of the variable to load
            O_ILOAD,
            O_STORE,
            O_DEL,

            O_DEF,
            O_CALL,
            // [O_NCALL, keys, args...] -- dot-chain callee, keys resolved at call time
            O_NCALL,
            // [O_ICALL, callee, args...] -- @f(...), callee is an expression naming the function
            O_ICALL,
            // return f(...) inside f itself -- falls back to O_CALL when the name resolves elsewhere
            O_TCALL,
            O_RETURN,
            // [O_UNPACK, arg] -- f(x[]): spreads a vec/lst into separate arguments
            O_UNPACK,

            O_IF,
            O_WHILE,
            O_FOR,
            O_FOREACH,
            O_BREAK,
            O_CONTINUE,
            O_SWITCH,
            O_CASE,
            O_DEFAULT,

            O_TRY,
            O_CATCH,
            O_THROW,

            O_IMPORT,
            O_LINK,
            O_ENV,
            // [O_EXCALL, name, args...] -- $f(...)
            O_EXCALL,
            // [O_ECONST, name] -- bare $f, checked against the host's definition table at parse time
            O_ECONST,

            // O_CURRENT / O_PARENT / O_ROOT are steps inside a dot chain, never values on their own
            O_CURRENT,
            O_PARENT,
            O_ROOT,
            O_DOT,
            O_INDEX,
            O_SLICE,

            O_INT,
            O_FLOAT,
            O_STRING,
            O_BOOL,
            O_BYTES,
            O_VEC,
            O_MAP,
            O_LST,
            O_TYPE,
            // [O_HERE, map] -- the map carries row / col / ofst / file as here() saw them
            O_HERE,
            // [O_TRAP, here, args?] or [O_TRAP, here, cond, args?] -- the hook fires when cond is absent or true
            O_TRAP,
            // [O_EVAL, code, params?] -- eval(): code is lexed and walked at run time in an anonymous frame
            O_EVAL,

            O_UPLUS,
            O_UMINUS,
            O_PRE_INC,
            O_PRE_DEC,
            O_POST_INC,
            O_POST_DEC,

            O_ADD,
            O_SUB,
            O_MUL,
            O_DIV,
            O_MOD,

            O_LSHIFT,
            O_RSHIFT,
            O_BIT_AND,
            O_BIT_OR,
            O_BIT_XOR,
            O_BIT_NEG,

            O_EQ,
            O_NE,
            O_LT,
            O_GT,
            O_LE,
            O_GE,

            O_AND,
            O_OR,
            O_NOT,

            O_TERNARY,
            O_COMMA,

            // Only the parse-time purity pass builds these; every other `a op= b` stays desugared into O_STORE
            O_ASS_ADD,
            O_ASS_SUB,
            O_ASS_MUL,
            O_ASS_DIV,
            O_ASS_MOD,
            O_ASS_LSHIFT,
            O_ASS_RSHIFT,
            O_ASS_BIT_AND,
            O_ASS_BIT_OR,
            O_ASS_BIT_XOR,

            // [O_DEBUG, row, col, ofst] -- not an instruction: no s_ops entry, dispatched before the hook checkpoint
            O_DEBUG,

            // Bound for the s_ops table; ass_op_for also returns it to mean "not a compound op"
            O_ENUMSIZE
        };

        // Indexed by opcode value -- the static_assert below checks the count, not the order
        constexpr const char* s_op_names[] = {
            "NOP",
            "PROGRAM",
            "BLOCK",
            "VAR",
            "LOAD",
            "ILOAD",
            "STORE",
            "DEL",
            "DEF",
            "CALL",
            "NCALL",
            "ICALL",
            "TCALL",
            "RETURN",
            "UNPACK",
            "IF",
            "WHILE",
            "FOR",
            "FOREACH",
            "BREAK",
            "CONTINUE",
            "SWITCH",
            "CASE",
            "DEFAULT",
            "TRY",
            "CATCH",
            "THROW",
            "IMPORT",
            "LINK",
            "ENV",
            "EXCALL",
            "ECONST",
            "CURRENT",
            "PARENT",
            "ROOT",
            "DOT",
            "INDEX",
            "SLICE",
            "INT",
            "FLOAT",
            "STRING",
            "BOOL",
            "BYTES",
            "VEC",
            "MAP",
            "LST",
            "TYPE",
            "HERE",
            "TRAP",
            "EVAL",
            "UPLUS",
            "UMINUS",
            "PRE_INC",
            "PRE_DEC",
            "POST_INC",
            "POST_DEC",
            "ADD",
            "SUB",
            "MUL",
            "DIV",
            "MOD",
            "LSHIFT",
            "RSHIFT",
            "BIT_AND",
            "BIT_OR",
            "BIT_XOR",
            "BIT_NEG",
            "EQ",
            "NE",
            "LT",
            "GT",
            "LE",
            "GE",
            "AND",
            "OR",
            "NOT",
            "TERNARY",
            "COMMA",
            "ASS_ADD",
            "ASS_SUB",
            "ASS_MUL",
            "ASS_DIV",
            "ASS_MOD",
            "ASS_LSHIFT",
            "ASS_RSHIFT",
            "ASS_BIT_AND",
            "ASS_BIT_OR",
            "ASS_BIT_XOR",
            "DEBUG",
        };
        static_assert(sizeof(s_op_names) / sizeof(s_op_names[0]) == O_ENUMSIZE,
                      "s_op_names must match op_enum count");

        using TKTYPE = uint_8;
        enum tk_enum : TKTYPE {

            T_INT_LITERAL,
            T_HEX_LITERAL,
            T_OCT_LITERAL,
            T_BIN_LITERAL,
            T_FLOAT_LITERAL,
            T_STRING_LITERAL,
            // `...`: raw text, no escape processing, newlines allowed
            T_BACKTICK_STRING,

            T_NAME,

            // T_VAR..T_EVAL must stay contiguous -- the parser takes any token in this range as a $name
            T_VAR,
            T_DEF,
            T_IF,
            T_ELSE,
            T_WHILE,
            T_FOR,
            T_BREAK,
            T_CONTINUE,
            T_RETURN,
            T_IMPORT,
            T_LINK,
            T_AS,
            T_TRY,
            T_CATCH,
            T_DELETE,
            T_THROW,
            T_SWITCH,
            T_CASE,
            T_DEFAULT,

            T_TRUE,
            T_FALSE,
            T_NULL_,
            T_NAN,
            T_INF,

            T_INT,
            T_FLOAT,
            T_STRING,
            T_BOOL,
            T_BYTES,
            T_VEC,
            T_MAP,
            T_LST,
            T_TYPE,
            T_ENV,
            T_HERE,
            T_TRAP,
            T_EVAL,

            T_PLUS,
            T_MINUS,
            T_STAR,
            T_SLASH,
            T_PERCENT,

            T_AUTO_INC,
            T_AUTO_DEC,

            T_LSHIFT,
            T_RSHIFT,

            T_LT,
            T_GT,
            T_LE,
            T_GE,
            T_EQ,
            T_NE,

            T_BIT_AND,
            T_BIT_OR,
            T_BIT_XOR,
            T_BIT_NEG,

            T_AND,
            T_OR,
            T_NOT,

            T_QUESTION,
            T_COLON,

            T_ASS,
            T_ASS_ADD,
            T_ASS_MINUS,
            T_ASS_MUL,
            T_ASS_DIV,
            T_ASS_MOD,
            T_ASS_LSHIFT,
            T_ASS_RSHIFT,
            T_ASS_BIT_AND,
            T_ASS_BIT_OR,
            T_ASS_BIT_XOR,

            T_COMMA,
            T_LP,
            T_RP,
            T_LC,
            T_RC,
            T_LB,
            T_RB,
            T_SEMICOLON,
            T_DOT,
            T_DOTDOT,
            T_COLONCOLON,
            T_AT,
            T_DOLLAR,

            // Kept with its delimiters so prtfmt can re-emit it; the parser's lookahead skips it and T_ERROR
            T_COMMENT,

            T_END_OF_FILE,
            // The lexer recovers and keeps lexing; parse() rejects the source once any T_ERROR was emitted
            T_ERROR
        };

    }
}

#endif
