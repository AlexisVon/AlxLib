/*****************************************************************/ /**
 * \file   gt_ascript_lex.cpp
 * \brief  Lexer unit tests — full token coverage
 *
 * \author alexis
 * Copyright (c) 2026 AlexisVon
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "ascript_lex.h"
#include <gtest/gtest.h>

using namespace alx;
using namespace alx::script;

static token_list lex(const char* src) {
    token_list tl;
    alx::bytes b(src);
    bool ok = tl.tokenize(alx::bytes_view(b));
    EXPECT_TRUE(ok) << "Lexer reported error for: " << src;
    return tl;
}

static token_list lex_err(const char* src) {
    token_list tl;
    alx::bytes b(src);
    tl.tokenize(alx::bytes_view(b));
    return tl;
}

#define EXPECT_TOK(tl, idx, expected_type) \
    EXPECT_EQ(tl.type(idx), expected_type) \
        << "token[" << idx << "] type mismatch"

#define EXPECT_TEXT(tl, idx, expected)                        \
    EXPECT_EQ(tl.text(idx), bytes_view(alx::bytes(expected))) \
        << "token[" << idx << "] text mismatch"

TEST(gt_ascript_lex, IntLiteralDecimal) {
    auto t = lex("123");
    EXPECT_TOK(t, 0, T_INT_LITERAL);
    EXPECT_TEXT(t, 0, "123");
    EXPECT_TOK(t, 1, T_END_OF_FILE);
}

TEST(gt_ascript_lex, IntLiteralHex) {
    auto t = lex("0xFF");
    EXPECT_TOK(t, 0, T_HEX_LITERAL);
    EXPECT_TEXT(t, 0, "0xFF");
}

TEST(gt_ascript_lex, IntLiteralOctal) {
    auto t = lex("0o77");
    EXPECT_TOK(t, 0, T_OCT_LITERAL);
    EXPECT_TEXT(t, 0, "0o77");
}

TEST(gt_ascript_lex, IntLiteralBinary) {
    auto t = lex("0b1010");
    EXPECT_TOK(t, 0, T_BIN_LITERAL);
    EXPECT_TEXT(t, 0, "0b1010");
}

TEST(gt_ascript_lex, FloatLiteral) {
    auto t = lex("3.14");
    EXPECT_TOK(t, 0, T_FLOAT_LITERAL);
    EXPECT_TEXT(t, 0, "3.14");
}

TEST(gt_ascript_lex, FloatLiteralExp) {
    auto t = lex("1e5");
    EXPECT_TOK(t, 0, T_FLOAT_LITERAL);
    EXPECT_TEXT(t, 0, "1e5");
}

TEST(gt_ascript_lex, CharLiteral) {
    auto t = lex("'c'");
    EXPECT_TOK(t, 0, T_CHAR_LITERAL);
    EXPECT_TEXT(t, 0, "'c'");
}

TEST(gt_ascript_lex, CharLiteralEscaped) {
    auto t = lex("'\\n'");
    EXPECT_TOK(t, 0, T_CHAR_LITERAL);
    EXPECT_TEXT(t, 0, "'\\n'");
}

TEST(gt_ascript_lex, StringLiteral) {
    auto t = lex("\"hello\"");
    EXPECT_TOK(t, 0, T_STRING_LITERAL);
    EXPECT_TEXT(t, 0, "\"hello\"");
}

TEST(gt_ascript_lex, StringLiteralEscaped) {
    auto t = lex("\"esc\\n\"");
    EXPECT_TOK(t, 0, T_STRING_LITERAL);
    EXPECT_TEXT(t, 0, "\"esc\\n\"");
}

TEST(gt_ascript_lex, KeywordVar) {
    auto t = lex("var");
    EXPECT_TOK(t, 0, T_VAR);
}
TEST(gt_ascript_lex, KeywordDef) {
    auto t = lex("def");
    EXPECT_TOK(t, 0, T_DEF);
}
TEST(gt_ascript_lex, KeywordIf) {
    auto t = lex("if");
    EXPECT_TOK(t, 0, T_IF);
}
TEST(gt_ascript_lex, KeywordElse) {
    auto t = lex("else");
    EXPECT_TOK(t, 0, T_ELSE);
}
TEST(gt_ascript_lex, KeywordHere) {
    auto t = lex("here");
    EXPECT_TOK(t, 0, T_HERE);
}
TEST(gt_ascript_lex, KeywordHereParens) {
    auto t = lex("here()");
    EXPECT_TOK(t, 0, T_HERE);
    EXPECT_TOK(t, 1, T_LP);
    EXPECT_TOK(t, 2, T_RP);
}
TEST(gt_ascript_lex, KeywordWhile) {
    auto t = lex("while");
    EXPECT_TOK(t, 0, T_WHILE);
}
TEST(gt_ascript_lex, KeywordFor) {
    auto t = lex("for");
    EXPECT_TOK(t, 0, T_FOR);
}
TEST(gt_ascript_lex, KeywordBreak) {
    auto t = lex("break");
    EXPECT_TOK(t, 0, T_BREAK);
}
TEST(gt_ascript_lex, KeywordContinue) {
    auto t = lex("continue");
    EXPECT_TOK(t, 0, T_CONTINUE);
}
TEST(gt_ascript_lex, KeywordReturn) {
    auto t = lex("return");
    EXPECT_TOK(t, 0, T_RETURN);
}
TEST(gt_ascript_lex, KeywordImport) {
    auto t = lex("import");
    EXPECT_TOK(t, 0, T_IMPORT);
}
TEST(gt_ascript_lex, KeywordLink) {
    auto t = lex("link");
    EXPECT_TOK(t, 0, T_LINK);
}
TEST(gt_ascript_lex, KeywordTry) {
    auto t = lex("try");
    EXPECT_TOK(t, 0, T_TRY);
}
TEST(gt_ascript_lex, KeywordCatch) {
    auto t = lex("catch");
    EXPECT_TOK(t, 0, T_CATCH);
}
TEST(gt_ascript_lex, KeywordThrow) {
    auto t = lex("throw");
    EXPECT_TOK(t, 0, T_THROW);
}
TEST(gt_ascript_lex, KeywordSwitch) {
    auto t = lex("switch");
    EXPECT_TOK(t, 0, T_SWITCH);
}
TEST(gt_ascript_lex, KeywordCase) {
    auto t = lex("case");
    EXPECT_TOK(t, 0, T_CASE);
}
TEST(gt_ascript_lex, KeywordDefault) {
    auto t = lex("default");
    EXPECT_TOK(t, 0, T_DEFAULT);
}

TEST(gt_ascript_lex, ConstantTrue) {
    auto t = lex("true");
    EXPECT_TOK(t, 0, T_TRUE);
}
TEST(gt_ascript_lex, ConstantFalse) {
    auto t = lex("false");
    EXPECT_TOK(t, 0, T_FALSE);
}
TEST(gt_ascript_lex, ConstantNull) {
    auto t = lex("null");
    EXPECT_TOK(t, 0, T_NULL_);
}

TEST(gt_ascript_lex, IdentifierSimple) {
    auto t = lex("foo");
    EXPECT_TOK(t, 0, T_NAME);
    EXPECT_TEXT(t, 0, "foo");
}

TEST(gt_ascript_lex, IdentifierWithUnderscore) {
    auto t = lex("_var2");
    EXPECT_TOK(t, 0, T_NAME);
    EXPECT_TEXT(t, 0, "_var2");
}

TEST(gt_ascript_lex, OpPlus) {
    auto t = lex("+");
    EXPECT_TOK(t, 0, T_PLUS);
}
TEST(gt_ascript_lex, OpAutoInc) {
    auto t = lex("++");
    EXPECT_TOK(t, 0, T_AUTO_INC);
}
TEST(gt_ascript_lex, OpAssAdd) {
    auto t = lex("+=");
    EXPECT_TOK(t, 0, T_ASS_ADD);
}
TEST(gt_ascript_lex, OpMinus) {
    auto t = lex("-");
    EXPECT_TOK(t, 0, T_MINUS);
}
TEST(gt_ascript_lex, OpAutoDec) {
    auto t = lex("--");
    EXPECT_TOK(t, 0, T_AUTO_DEC);
}
TEST(gt_ascript_lex, OpAssMinus) {
    auto t = lex("-=");
    EXPECT_TOK(t, 0, T_ASS_MINUS);
}
TEST(gt_ascript_lex, OpStar) {
    auto t = lex("*");
    EXPECT_TOK(t, 0, T_STAR);
}
TEST(gt_ascript_lex, OpAssMul) {
    auto t = lex("*=");
    EXPECT_TOK(t, 0, T_ASS_MUL);
}
TEST(gt_ascript_lex, OpSlash) {
    auto t = lex("/");
    EXPECT_TOK(t, 0, T_SLASH);
}
TEST(gt_ascript_lex, OpAssDiv) {
    auto t = lex("/=");
    EXPECT_TOK(t, 0, T_ASS_DIV);
}
TEST(gt_ascript_lex, OpPercent) {
    auto t = lex("%");
    EXPECT_TOK(t, 0, T_PERCENT);
}
TEST(gt_ascript_lex, OpAssMod) {
    auto t = lex("%=");
    EXPECT_TOK(t, 0, T_ASS_MOD);
}
TEST(gt_ascript_lex, OpLt) {
    auto t = lex("<");
    EXPECT_TOK(t, 0, T_LT);
}
TEST(gt_ascript_lex, OpLshift) {
    auto t = lex("<<");
    EXPECT_TOK(t, 0, T_LSHIFT);
}
TEST(gt_ascript_lex, OpLe) {
    auto t = lex("<=");
    EXPECT_TOK(t, 0, T_LE);
}
TEST(gt_ascript_lex, OpAssLshift) {
    auto t = lex("<<=");
    EXPECT_TOK(t, 0, T_ASS_LSHIFT);
}
TEST(gt_ascript_lex, OpGt) {
    auto t = lex(">");
    EXPECT_TOK(t, 0, T_GT);
}
TEST(gt_ascript_lex, OpRshift) {
    auto t = lex(">>");
    EXPECT_TOK(t, 0, T_RSHIFT);
}
TEST(gt_ascript_lex, OpGe) {
    auto t = lex(">=");
    EXPECT_TOK(t, 0, T_GE);
}
TEST(gt_ascript_lex, OpAssRshift) {
    auto t = lex(">>=");
    EXPECT_TOK(t, 0, T_ASS_RSHIFT);
}
TEST(gt_ascript_lex, OpAss) {
    auto t = lex("=");
    EXPECT_TOK(t, 0, T_ASS);
}
TEST(gt_ascript_lex, OpEq) {
    auto t = lex("==");
    EXPECT_TOK(t, 0, T_EQ);
}
TEST(gt_ascript_lex, OpNot) {
    auto t = lex("!");
    EXPECT_TOK(t, 0, T_NOT);
}
TEST(gt_ascript_lex, OpNe) {
    auto t = lex("!=");
    EXPECT_TOK(t, 0, T_NE);
}
TEST(gt_ascript_lex, OpBitAnd) {
    auto t = lex("&");
    EXPECT_TOK(t, 0, T_BIT_AND);
}
TEST(gt_ascript_lex, OpAnd) {
    auto t = lex("&&");
    EXPECT_TOK(t, 0, T_AND);
}
TEST(gt_ascript_lex, OpAssBitAnd) {
    auto t = lex("&=");
    EXPECT_TOK(t, 0, T_ASS_BIT_AND);
}
TEST(gt_ascript_lex, OpBitOr) {
    auto t = lex("|");
    EXPECT_TOK(t, 0, T_BIT_OR);
}
TEST(gt_ascript_lex, OpOr) {
    auto t = lex("||");
    EXPECT_TOK(t, 0, T_OR);
}
TEST(gt_ascript_lex, OpAssBitOr) {
    auto t = lex("|=");
    EXPECT_TOK(t, 0, T_ASS_BIT_OR);
}
TEST(gt_ascript_lex, OpBitXor) {
    auto t = lex("^");
    EXPECT_TOK(t, 0, T_BIT_XOR);
}
TEST(gt_ascript_lex, OpAssBitXor) {
    auto t = lex("^=");
    EXPECT_TOK(t, 0, T_ASS_BIT_XOR);
}
TEST(gt_ascript_lex, OpBitNeg) {
    auto t = lex("~");
    EXPECT_TOK(t, 0, T_BIT_NEG);
}
TEST(gt_ascript_lex, OpQuestion) {
    auto t = lex("?");
    EXPECT_TOK(t, 0, T_QUESTION);
}
TEST(gt_ascript_lex, OpColon) {
    auto t = lex(":");
    EXPECT_TOK(t, 0, T_COLON);
}

TEST(gt_ascript_lex, DelimComma) {
    auto t = lex(",");
    EXPECT_TOK(t, 0, T_COMMA);
}
TEST(gt_ascript_lex, DelimLP) {
    auto t = lex("(");
    EXPECT_TOK(t, 0, T_LP);
}
TEST(gt_ascript_lex, DelimRP) {
    auto t = lex(")");
    EXPECT_TOK(t, 0, T_RP);
}
TEST(gt_ascript_lex, DelimLC) {
    auto t = lex("{");
    EXPECT_TOK(t, 0, T_LC);
}
TEST(gt_ascript_lex, DelimRC) {
    auto t = lex("}");
    EXPECT_TOK(t, 0, T_RC);
}
TEST(gt_ascript_lex, DelimLB) {
    auto t = lex("[");
    EXPECT_TOK(t, 0, T_LB);
}
TEST(gt_ascript_lex, DelimRB) {
    auto t = lex("]");
    EXPECT_TOK(t, 0, T_RB);
}
TEST(gt_ascript_lex, DelimSemicolon) {
    auto t = lex(";");
    EXPECT_TOK(t, 0, T_SEMICOLON);
}
TEST(gt_ascript_lex, DelimDot) {
    auto t = lex(".");
    EXPECT_TOK(t, 0, T_DOT);
}

TEST(gt_ascript_lex, LineComment) {
    auto t = lex("var x = 1; // comment\nvar y = 2;");
    EXPECT_EQ(t.count(), 12u);
    EXPECT_TOK(t, 0, T_VAR);
    EXPECT_TOK(t, 5, T_COMMENT);
    EXPECT_TEXT(t, 5, "// comment");
    EXPECT_TOK(t, 6, T_VAR);
}

TEST(gt_ascript_lex, BlockComment) {
    auto t = lex("/* block */ var x = 1;");
    EXPECT_EQ(t.count(), 7u);
    EXPECT_TOK(t, 0, T_COMMENT);
    EXPECT_TEXT(t, 0, "/* block */");
    EXPECT_TOK(t, 1, T_VAR);
    EXPECT_TEXT(t, 1, "var");
}

TEST(gt_ascript_lex, WhitespaceNewlines) {
    auto t = lex("var\n  x\n    =  1;");
    EXPECT_TOK(t, 0, T_VAR);
    EXPECT_EQ(t.row(0), 1u);
    EXPECT_EQ(t.col(0), 1u);
    EXPECT_TOK(t, 1, T_NAME);
    EXPECT_EQ(t.row(1), 2u);
    EXPECT_EQ(t.col(1), 3u);
}

TEST(gt_ascript_lex, PositionTracking) {
    auto t = lex("var x = 1;");

    EXPECT_EQ(t.row(0), 1u);
    EXPECT_EQ(t.col(0), 1u);
    EXPECT_EQ(t.ofst(0), 0u);
    EXPECT_EQ(t.size(0), 3u);

    EXPECT_EQ(t.row(1), 1u);
    EXPECT_EQ(t.col(1), 5u);
    EXPECT_EQ(t.ofst(1), 4u);
    EXPECT_EQ(t.size(1), 1u);

    EXPECT_EQ(t.row(2), 1u);
    EXPECT_EQ(t.col(2), 7u);
    EXPECT_EQ(t.ofst(2), 6u);
    EXPECT_EQ(t.size(2), 1u);

    EXPECT_EQ(t.row(3), 1u);
    EXPECT_EQ(t.col(3), 9u);

    EXPECT_EQ(t.row(4), 1u);
    EXPECT_EQ(t.col(4), 10u);

    EXPECT_EQ(t.row(5), 1u);
    EXPECT_EQ(t.col(5), 11u);
    EXPECT_EQ(t.size(5), 0u);
}

TEST(gt_ascript_lex, InvalidCharAt) {
    auto t = lex_err("`");
    EXPECT_TOK(t, 0, T_ERROR);
}

TEST(gt_ascript_lex, InvalidCharBacktick) {
    auto t = lex_err("`");
    EXPECT_TOK(t, 0, T_ERROR);
}

TEST(gt_ascript_lex, EmptyInput) {
    auto t = lex("");
    EXPECT_EQ(t.count(), 1u);
    EXPECT_TOK(t, 0, T_END_OF_FILE);
}

TEST(gt_ascript_lex, FullSample) {
    auto t = lex(
        "var x = 123;\n"
        "var y = 0xFF;\n"
        "if (x > 0) { x = x + 1; }\n");
    EXPECT_TRUE(t.count() > 10);
    EXPECT_TOK(t, t.count() - 1, T_END_OF_FILE);
}

TEST(gt_ascript_lex, UppercaseHex) {
    auto t = lex("0XFF");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_HEX_LITERAL);
}

TEST(gt_ascript_lex, UppercaseOctal) {
    auto t = lex("0O77");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_OCT_LITERAL);
}

TEST(gt_ascript_lex, UppercaseBinary) {
    auto t = lex("0B1010");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_BIN_LITERAL);
}

TEST(gt_ascript_lex, UppercaseExponent) {
    auto t = lex("1E5");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_FLOAT_LITERAL);
}

TEST(gt_ascript_lex, UnterminatedString) {
    auto t = lex_err("\"hello");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_ERROR);
}

TEST(gt_ascript_lex, UnterminatedBlockComment) {
    auto t = lex_err("/* unclosed");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_ERROR);
}

TEST(gt_ascript_lex, UnterminatedSingleQuote) {
    auto t = lex_err("'x");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_ERROR);
}

TEST(gt_ascript_lex, ErrorCallback_UnterminatedChar) {
    token_list tl;
    alx::bytes b("'x");
    std::vector<compile_error> errors;
    alx::signal<const compile_error&> err_sig;
    err_sig.connect([&](const compile_error& e) { errors.push_back(e); });
    tl.tokenize(alx::bytes_view(b), &err_sig);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].msg, "unterminated character literal");
}

TEST(gt_ascript_lex, EmptyCharLiteral) {

    auto t = lex("''");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_CHAR_LITERAL);
}

TEST(gt_ascript_lex, CharLiteralEscapes) {

    auto t = lex("'\\t' '\\r' '\\\\' '\\'' '\\\"' '\\0' '\\x41'");
    EXPECT_EQ(t.count(), 8u);
    for (int i = 0; i < 7; i++) EXPECT_TOK(t, i, T_CHAR_LITERAL);
}

TEST(gt_ascript_lex, StringEscapes) {
    auto t = lex("\"\\t\\r\\n\\\\\\\"\"");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_STRING_LITERAL);
}

TEST(gt_ascript_lex, EmptyString) {
    auto t = lex("\"\"");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_STRING_LITERAL);
}

TEST(gt_ascript_lex, TabInString) {
    auto t = lex("\"a\tb\"");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_STRING_LITERAL);
}

TEST(gt_ascript_lex, SingleZero) {
    auto t = lex("0");
    EXPECT_EQ(t.count(), 2u);
    EXPECT_TOK(t, 0, T_INT_LITERAL);
}

TEST(gt_ascript_lex, ErrorCallback_InvalidChar) {
    token_list tl;
    alx::bytes b("\\");
    std::vector<compile_error> errors;
    alx::signal<const compile_error&> err_sig;
    err_sig.connect([&](const compile_error& e) { errors.push_back(e); });
    tl.tokenize(alx::bytes_view(b), &err_sig);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].msg, "invalid character: '\\'");
    EXPECT_EQ(errors[0].loc.row, 1u);
    EXPECT_EQ(errors[0].loc.index, 0u);
}

TEST(gt_ascript_lex, ErrorCallback_UnterminatedString) {
    token_list tl;
    alx::bytes b("\"hello");
    std::vector<compile_error> errors;
    alx::signal<const compile_error&> err_sig;
    err_sig.connect([&](const compile_error& e) { errors.push_back(e); });
    tl.tokenize(alx::bytes_view(b), &err_sig);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].msg, "unterminated string literal");
}

TEST(gt_ascript_lex, Utf8Column_Ascii) {
    token_list tl;
    alx::bytes b("`abc");
    std::vector<compile_error> errors;
    alx::signal<const compile_error&> err_sig;
    err_sig.connect([&](const compile_error& e) { errors.push_back(e); });
    tl.tokenize(alx::bytes_view(b), &err_sig);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].loc.col, 1u);
}

TEST(gt_ascript_lex, Utf8Column_InString) {

    const char src[] = "\"\xe4\xbd\xa0\xe5\xa5\xbd@";
    token_list tl;
    alx::bytes b(src, sizeof(src) - 1);
    std::vector<compile_error> errors;
    alx::signal<const compile_error&> err_sig;
    err_sig.connect([&](const compile_error& e) { errors.push_back(e); });
    tl.tokenize(alx::bytes_view(b), &err_sig);
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_EQ(errors[0].msg, "unterminated string literal");
    EXPECT_EQ(errors[0].loc.col, 1u);
    EXPECT_EQ(errors[0].loc.index, 0u);
}

TEST(gt_ascript_lex, BomSkip) {
    char src[] = "\xEF\xBB\xBF"
                 "var x = 1;";
    token_list tl;
    alx::bytes b(src, 12);
    bool ok = tl.tokenize(alx::bytes_view(b));
    EXPECT_TRUE(ok);

    EXPECT_EQ(tl.text(0).to_string(), "var");
}

TEST(gt_ascript_lex, BuiltinFuncTokens) {

    struct {
        const char* name;
        tk_enum expected;
    } builtins[] = {
        {"int", T_INT},
        {"float", T_FLOAT},
        {"string", T_STRING},
        {"bool", T_BOOL},
        {"vec", T_VEC},
        {"map", T_MAP},
        {"lst", T_LST},
        {"type", T_TYPE},
        {"env", T_ENV},
    };
    for (auto& b : builtins) {
        token_list tl;
        tl.tokenize(alx::bytes_view(alx::bytes(b.name)));
        EXPECT_EQ(tl.type(0), b.expected) << "failed for: " << b.name;
        EXPECT_EQ(tl.text(0).to_string(), b.name);
    }
}

TEST(gt_ascript_lex, Exfunc_Basic) {
    token_list tl;
    tl.tokenize(alx::bytes_view(alx::bytes("$foo")));
    ASSERT_GE(tl.count(), 3u);
    EXPECT_EQ(tl.type(0), T_DOLLAR);
    EXPECT_EQ(tl.type(1), T_NAME);
    EXPECT_EQ(tl.text(1).to_string(), "foo");
}

TEST(gt_ascript_lex, Exfunc_WithArgs) {
    token_list tl;
    tl.tokenize(alx::bytes_view(alx::bytes("$foo(x, y)")));
    ASSERT_GE(tl.count(), 8u);
    EXPECT_EQ(tl.type(0), T_DOLLAR);
    EXPECT_EQ(tl.type(1), T_NAME);
    EXPECT_EQ(tl.text(1).to_string(), "foo");
    EXPECT_EQ(tl.type(2), T_LP);
    EXPECT_EQ(tl.type(3), T_NAME);
    EXPECT_EQ(tl.type(4), T_COMMA);
    EXPECT_EQ(tl.type(5), T_NAME);
    EXPECT_EQ(tl.type(6), T_RP);
}

TEST(gt_ascript_lex, Exfunc_UnderscoreName) {
    token_list tl;
    tl.tokenize(alx::bytes_view(alx::bytes("$foo_bar")));
    ASSERT_GE(tl.count(), 3u);
    EXPECT_EQ(tl.type(0), T_DOLLAR);
    EXPECT_EQ(tl.type(1), T_NAME);
    EXPECT_EQ(tl.text(1).to_string(), "foo_bar");
}

TEST(gt_ascript_lex, Exfunc_BareDollar) {

    token_list tl;
    tl.tokenize(alx::bytes_view(alx::bytes("$")));
    ASSERT_GE(tl.count(), 2u);
    EXPECT_EQ(tl.type(0), T_DOLLAR);
}

TEST(gt_ascript_lex, Exfunc_KeywordName) {

    token_list tl;
    tl.tokenize(alx::bytes_view(alx::bytes("$var")));
    ASSERT_GE(tl.count(), 3u);
    EXPECT_EQ(tl.type(0), T_DOLLAR);
    EXPECT_EQ(tl.type(1), T_VAR);
    EXPECT_EQ(tl.text(1).to_string(), "var");
}

TEST(gt_ascript_lex, Exfunc_DoubleUnderscoreIsName) {

    token_list tl;
    tl.tokenize(alx::bytes_view(alx::bytes("__foo__")));
    ASSERT_GE(tl.count(), 2u);
    EXPECT_EQ(tl.type(0), T_NAME);
    EXPECT_EQ(tl.text(0).to_string(), "__foo__");
}

TEST(gt_ascript_lex, Exfunc_RegularUnderscore) {

    token_list tl;
    tl.tokenize(alx::bytes_view(alx::bytes("_foo")));
    EXPECT_EQ(tl.type(0), T_NAME);
}
