#pragma once
#ifndef PYTHON_INTERPRETER_EVALVISITOR_H
#define PYTHON_INTERPRETER_EVALVISITOR_H

#include "Python3ParserBaseVisitor.h"
#include "Python3Parser.h"
#include <any>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <iomanip>

struct BigInt {
	static const int BASE = 1000000000;
	std::vector<int> d; // little-endian digits
	bool neg = false;
	BigInt() = default;
	BigInt(long long v) { *this = fromLL(v); }
	static BigInt fromLL(long long v) {
		BigInt x; if (v < 0) { x.neg = true; v = -v; }
		while (v) { x.d.push_back(int(v % BASE)); v /= BASE; }
		return x.normalize();
	}
	static BigInt fromString(const std::string &s);
	std::string toString() const;
	int cmpAbs(const BigInt &b) const;
	BigInt &normalize();
	static BigInt addAbs(const BigInt &a, const BigInt &b);
	static BigInt subAbs(const BigInt &a, const BigInt &b); // assume |a|>=|b|
	static BigInt mul(const BigInt &a, const BigInt &b);
	static std::pair<BigInt, BigInt> divmod(const BigInt &a, const BigInt &b); // b!=0
	static BigInt add(const BigInt &a, const BigInt &b);
	static BigInt sub(const BigInt &a, const BigInt &b);
};

struct Value {
	enum Type { TNone, TInt, TFloat, TStr, TBool, TTuple, TFunc } type = TNone;
	BigInt i;
	double f = 0.0;
	std::string s;
	bool b = false;
	std::vector<Value> tup;
	// Function representation
	struct Func {
		std::vector<std::string> params;
		std::vector<bool> hasDefault;
		std::vector<Python3Parser::TestContext*> defaults; // same size as params
		Python3Parser::SuiteContext* body = nullptr;
	};
	Func func;

	static Value None() { return Value(); }
	static Value fromInt(const BigInt &x) { Value v; v.type=TInt; v.i=x; return v; }
	static Value fromFloat(double x) { Value v; v.type=TFloat; v.f=x; return v; }
	static Value fromStr(std::string x) { Value v; v.type=TStr; v.s=std::move(x); return v; }
	static Value fromBool(bool x) { Value v; v.type=TBool; v.b=x; return v; }
	static Value fromTuple(std::vector<Value> x) { Value v; v.type=TTuple; v.tup=std::move(x); return v; }
	static Value fromFunc(Func f) { Value v; v.type=TFunc; v.func=std::move(f); return v; }
};

struct ReturnSignal { std::vector<Value> values; };
struct BreakSignal {};
struct ContinueSignal {};

class EvalVisitor : public Python3ParserBaseVisitor {
public:
	std::unordered_map<std::string, Value> globals;
	std::vector<std::unordered_map<std::string, Value>> localsStack; // top is current function scope

	std::any visitFile_input(Python3Parser::File_inputContext *ctx) override;
	std::any visitStmt(Python3Parser::StmtContext *ctx) override;
	std::any visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) override;
	std::any visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) override;
	std::any visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) override;
	std::any visitAugassign(Python3Parser::AugassignContext *ctx) override;
	std::any visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) override;
	std::any visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) override;
	std::any visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) override;
	std::any visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) override;
	std::any visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) override;
	std::any visitIf_stmt(Python3Parser::If_stmtContext *ctx) override;
	std::any visitWhile_stmt(Python3Parser::While_stmtContext *ctx) override;
	std::any visitSuite(Python3Parser::SuiteContext *ctx) override;
	std::any visitTest(Python3Parser::TestContext *ctx) override;
	std::any visitOr_test(Python3Parser::Or_testContext *ctx) override;
	std::any visitAnd_test(Python3Parser::And_testContext *ctx) override;
	std::any visitNot_test(Python3Parser::Not_testContext *ctx) override;
	std::any visitComparison(Python3Parser::ComparisonContext *ctx) override;
	std::any visitComp_op(Python3Parser::Comp_opContext *ctx) override;
	std::any visitArith_expr(Python3Parser::Arith_exprContext *ctx) override;
	std::any visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx) override;
	std::any visitTerm(Python3Parser::TermContext *ctx) override;
	std::any visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx) override;
	std::any visitFactor(Python3Parser::FactorContext *ctx) override;
	std::any visitAtom_expr(Python3Parser::Atom_exprContext *ctx) override;
	std::any visitTrailer(Python3Parser::TrailerContext *ctx) override;
	std::any visitAtom(Python3Parser::AtomContext *ctx) override;
	std::any visitFormat_string(Python3Parser::Format_stringContext *ctx) override;
	std::any visitTestlist(Python3Parser::TestlistContext *ctx) override;
	std::any visitArglist(Python3Parser::ArglistContext *ctx) override;
	std::any visitArgument(Python3Parser::ArgumentContext *ctx) override;
	std::any visitFuncdef(Python3Parser::FuncdefContext *ctx) override;
	std::any visitParameters(Python3Parser::ParametersContext *ctx) override;
	std::any visitTypedargslist(Python3Parser::TypedargslistContext *ctx) override;
	std::any visitTfpdef(Python3Parser::TfpdefContext *ctx) override;

private:
	Value evalTest(Python3Parser::TestContext *ctx);
	Value evalTestlistSingle(Python3Parser::TestlistContext *ctx);
	std::vector<Value> evalTestlist(Python3Parser::TestlistContext *ctx);
	Value evalAtomExpr(Python3Parser::Atom_exprContext *ctx);
	Value evalAtom(Python3Parser::AtomContext *ctx);
	Value evalCall(const std::string &name, Python3Parser::ArglistContext *args);
	Value getVar(const std::string &name);
	void setVar(const std::string &name, const Value &v);
	bool inFunction() const { return !localsStack.empty(); }
	bool truthy(const Value &v);
	Value toInt(const Value &v);
	Value toFloat(const Value &v);
	Value toStr(const Value &v);
	Value toBool(const Value &v);
	Value numAdd(const Value &a, const Value &b);
	Value numSub(const Value &a, const Value &b);
	Value numMul(const Value &a, const Value &b);
	Value numDiv(const Value &a, const Value &b); // float
	Value numIDiv(const Value &a, const Value &b); // int floor
	Value numMod(const Value &a, const Value &b);
	Value strMul(const std::string &s, const BigInt &times);
	int cmpNumbers(const Value &a, const Value &b);
};

#endif//PYTHON_INTERPRETER_EVALVISITOR_H
