#include "Evalvisitor.h"
using std::any;
using std::any_cast;
using std::string;
using std::vector;

static BigInt absBig(const BigInt &x){ BigInt r=x; r.neg=false; return r; }

BigInt &BigInt::normalize(){
	while(!d.empty() && d.back()==0) d.pop_back();
	if(d.empty()) neg=false;
	return *this;
}

int BigInt::cmpAbs(const BigInt &b) const{
	if(d.size()!=b.d.size()) return d.size()<b.d.size()?-1:1;
	for(int i=(int)d.size()-1;i>=0;--i){ if(d[i]!=b.d[i]) return d[i]<b.d[i]?-1:1; }
	return 0;
}

BigInt BigInt::fromString(const string &s){
	BigInt x; int i=0; bool neg=false; if(!s.empty() && s[0]=='-'){ neg=true; i=1; }
	for(int j=(int)s.size()-1;j>=i;j-=9){ int l=std::max(i,j-8); int v=0; for(int k=l;k<=j;k++) v=v*10+(s[k]-'0'); x.d.push_back(v); }
	x.neg=neg; return x.normalize();
}

string BigInt::toString() const{
	if(d.empty()) return "0";
	string s = neg?"-":"";
	s += std::to_string(d.back());
	for(int i=(int)d.size()-2;i>=0;--i){ string part=std::to_string(d[i]); s += string(9 - part.size(),'0') + part; }
	return s;
}

BigInt BigInt::addAbs(const BigInt &a,const BigInt &b){ BigInt r; long long carry=0; size_t n=std::max(a.d.size(),b.d.size()); r.d.resize(n); for(size_t i=0;i<n;i++){ long long av=i<a.d.size()?a.d[i]:0; long long bv=i<b.d.size()?b.d[i]:0; long long sum=av+bv+carry; r.d[i]=int(sum%BASE); carry=sum/BASE;} if(carry) r.d.push_back((int)carry); return r; }
BigInt BigInt::subAbs(const BigInt &a,const BigInt &b){ BigInt r; long long borrow=0; r.d.resize(a.d.size()); for(size_t i=0;i<a.d.size();i++){ long long av=a.d[i]; long long bv=i<b.d.size()?b.d[i]:0; long long diff=av-bv-borrow; if(diff<0){ diff+=BASE; borrow=1;} else borrow=0; r.d[i]=(int)diff; } return r.normalize(); }

BigInt BigInt::add(const BigInt &a,const BigInt &b){ if(a.neg==b.neg){ BigInt r=addAbs(absBig(a),absBig(b)); r.neg=a.neg; return r; } int cmp=absBig(a).cmpAbs(absBig(b)); if(cmp>=0){ BigInt r=subAbs(absBig(a),absBig(b)); r.neg=a.neg; return r; } else { BigInt r=subAbs(absBig(b),absBig(a)); r.neg=b.neg; return r; } }
BigInt BigInt::sub(const BigInt &a,const BigInt &b){ BigInt nb=b; nb.neg=!nb.neg; return add(a,nb); }

BigInt BigInt::mul(const BigInt &a,const BigInt &b){ BigInt r; r.d.assign(a.d.size()+b.d.size(),0); for(size_t i=0;i<a.d.size();++i){ long long carry=0; for(size_t j=0;j<b.d.size();++j){ long long cur=r.d[i+j]+(long long)a.d[i]*b.d[j]+carry; r.d[i+j]=(int)(cur%BASE); carry=cur/BASE; } size_t pos=i+b.d.size(); while(carry){ long long cur=r.d[pos]+carry; r.d[pos]=(int)(cur%BASE); carry=cur/BASE; ++pos; } } r.neg=a.neg^b.neg; return r.normalize(); }

std::pair<BigInt,BigInt> BigInt::divmod(const BigInt &a,const BigInt &b){ // naive long division, then adjust to floor semantics
	BigInt aa=absBig(a), bb=absBig(b); BigInt q, r; q.d.assign(aa.d.size(),0);
	for(int i=(int)aa.d.size()-1;i>=0;--i){ r.d.insert(r.d.begin(), aa.d[i]); r.normalize();
		int low=0, high=BASE-1, best=0; // binary search digit
		while(low<=high){ int mid=(low+high)/2; BigInt t; t.d={mid}; BigInt prod = BigInt::mul(bb, t); if(prod.cmpAbs(r)<=0){ best=mid; low=mid+1;} else high=mid-1; }
		q.d[i]=best; BigInt subp = BigInt::mul(bb, BigInt::fromLL(best)); r = BigInt::subAbs(r, subp);
	}
	q.normalize(); r.normalize(); q.neg = a.neg ^ b.neg;
	// floor adjustment for negative quotient
	if(q.neg && !r.d.empty()){
		q = BigInt::sub(q, BigInt::fromLL(1));
		// recompute remainder: r = a - q*b
		BigInt qb = BigInt::mul(b, q);
		r = BigInt::sub(a, qb).normalize();
	}
	else { r.neg = a.neg; }
	return {q,r};
}

static bool isInt(const Value &v){ return v.type==Value::TInt; }
static bool isFloat(const Value &v){ return v.type==Value::TFloat; }
static bool isBool(const Value &v){ return v.type==Value::TBool; }
static bool isStr(const Value &v){ return v.type==Value::TStr; }

any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx){
	for(auto st: ctx->stmt()){
		try{ visit(st); }catch(const BreakSignal&){/*ignore*/}catch(const ContinueSignal&){/*ignore*/}catch(const ReturnSignal&){/*ignore*/}
	}
	return {};
}

any EvalVisitor::visitStmt(Python3Parser::StmtContext *ctx){ return visit(ctx->simple_stmt()? (antlr4::tree::ParseTree*)ctx->simple_stmt() : (antlr4::tree::ParseTree*)ctx->compound_stmt()); }
any EvalVisitor::visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx){ return visit(ctx->small_stmt()); }
any EvalVisitor::visitSmall_stmt(Python3Parser::Small_stmtContext *ctx){ return visit(ctx->expr_stmt()? (antlr4::tree::ParseTree*)ctx->expr_stmt() : (antlr4::tree::ParseTree*)ctx->flow_stmt()); }

static BigInt toBig(const Value &v){ if(v.type==Value::TInt) return v.i; if(v.type==Value::TFloat){ long long ll=(long long)v.f; return BigInt::fromLL(ll);} if(v.type==Value::TBool) return BigInt::fromLL(v.b?1:0); if(v.type==Value::TStr){ BigInt bi; bool neg=false; size_t i=0; if(!v.s.empty()&&v.s[0]=='-'){neg=true; i=1;} for(;i<v.s.size();++i){ if(v.s[i]<'0'||v.s[i]>'9') return BigInt::fromLL(0); } bi=BigInt::fromString(v.s); bi.neg=neg; return bi; } return BigInt::fromLL(0); }
static double toDouble(const Value &v){ if(v.type==Value::TFloat) return v.f; if(v.type==Value::TInt){ return std::stod(v.i.toString()); } if(v.type==Value::TBool) return v.b?1.0:0.0; if(v.type==Value::TStr){ try{ return std::stod(v.s);}catch(...){ return 0.0;} } return 0.0; }

Value EvalVisitor::toInt(const Value &v){ return Value::fromInt(toBig(v)); }
Value EvalVisitor::toFloat(const Value &v){ return Value::fromFloat(toDouble(v)); }
Value EvalVisitor::toStr(const Value &v){
	if(v.type==Value::TStr) return v; if(v.type==Value::TInt) return Value::fromStr(v.i.toString()); if(v.type==Value::TFloat){ std::ostringstream os; os<<std::fixed<<std::setprecision(6)<<v.f; return Value::fromStr(os.str()); } if(v.type==Value::TBool) return Value::fromStr(v.b?"True":"False"); return Value::fromStr("None"); }
Value EvalVisitor::toBool(const Value &v){ if(v.type==Value::TBool) return v; if(v.type==Value::TInt) return Value::fromBool(!v.i.d.empty()); if(v.type==Value::TFloat) return Value::fromBool(v.f!=0.0); if(v.type==Value::TStr) return Value::fromBool(!v.s.empty()); return Value::fromBool(false); }

bool EvalVisitor::truthy(const Value &v){ return toBool(v).b; }

Value EvalVisitor::numAdd(const Value &a,const Value &b){ if(isStr(a)&&isStr(b)) return Value::fromStr(a.s+b.s); if(isFloat(a)||isFloat(b)) return Value::fromFloat(toDouble(a)+toDouble(b)); return Value::fromInt(BigInt::add(toBig(a), toBig(b))); }
Value EvalVisitor::numSub(const Value &a,const Value &b){ if(isFloat(a)||isFloat(b)) return Value::fromFloat(toDouble(a)-toDouble(b)); return Value::fromInt(BigInt::sub(toBig(a), toBig(b))); }
Value EvalVisitor::numMul(const Value &a,const Value &b){ if(isStr(a)&&isInt(b)) return strMul(a.s,toBig(b)); if(isStr(b)&&isInt(a)) return strMul(b.s,toBig(a)); if(isFloat(a)||isFloat(b)) return Value::fromFloat(toDouble(a)*toDouble(b)); return Value::fromInt(BigInt::mul(toBig(a), toBig(b))); }
Value EvalVisitor::numDiv(const Value &a,const Value &b){ return Value::fromFloat(toDouble(a)/toDouble(b)); }
Value EvalVisitor::numIDiv(const Value &a,const Value &b){ auto qr=BigInt::divmod(toBig(a), toBig(b)); return Value::fromInt(qr.first); }
Value EvalVisitor::numMod(const Value &a,const Value &b){ auto qr=BigInt::divmod(toBig(a), toBig(b)); return Value::fromInt(qr.second); }

Value EvalVisitor::strMul(const string &s,const BigInt &times){ // times >=0
	string out; string ts=times.toString(); long long t=std::stoll(ts); for(long long i=0;i<t;++i) out+=s; return Value::fromStr(out);
}

Value EvalVisitor::getVar(const string &name){ if(inFunction()){ auto &loc=localsStack.back(); auto it=loc.find(name); if(it!=loc.end()) return it->second; }
	auto it=globals.find(name); if(it!=globals.end()) return it->second; return Value::None(); }
void EvalVisitor::setVar(const string &name,const Value &v){ if(inFunction()){ auto &loc=localsStack.back(); auto it=loc.find(name); if(it!=loc.end()){ it->second=v; return; } }
	globals[name]=v; }

any EvalVisitor::visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx){ auto lists=ctx->testlist(); if(ctx->augassign()){ // a += expr
	Value left = evalTestlistSingle(lists[0]); Value right = evalTestlistSingle(lists[1]); // left must be name
	string name; if(auto ac = dynamic_cast<Python3Parser::Atom_exprContext*>(lists[0]->test(0)->or_test()->and_test(0)->not_test(0)->comparison()->arith_expr(0)->term(0)->factor(0)->atom_expr())){ if(ac->atom()->NAME()) name=ac->atom()->NAME()->getText(); }
	string op=ctx->augassign()->getText(); Value res; if(op=="+=") res=numAdd(left,right); else if(op=="-=") res=numSub(left,right); else if(op=="*=") res=numMul(left,right); else if(op=="/=") res=numDiv(left,right); else if(op=="//=") res=numIDiv(left,right); else if(op=="%=") res=numMod(left,right); setVar(name,res); return {}; }
	// assignment chain: a=b=...=expr or multiple assignment a,b=... handled left to right
	vector<Value> rights = evalTestlist(lists.back()); if(lists.size()>1){ // chained assigns
		Value last = rights.size()==1? rights[0]: Value::fromTuple(rights);
		for(size_t i=0;i<lists.size()-1;i++){ // each left is single NAME
			string name = lists[i]->test(0)->or_test()->and_test(0)->not_test(0)->comparison()->arith_expr(0)->term(0)->factor(0)->atom_expr()->atom()->NAME()->getText(); setVar(name,last);
		}
		return {};
	} else { // simple expr or multi assign
		if(lists.size()==1){ // expression-only statement
			( void ) evalTestlistSingle(lists[0]);
			return {};
		}
		// left list of names, right list values
		for(size_t i=0;i<lists[0]->test().size();++i){ string name = lists[0]->test(i)->or_test()->and_test(0)->not_test(0)->comparison()->arith_expr(0)->term(0)->factor(0)->atom_expr()->atom()->NAME()->getText(); Value v = rights[i]; setVar(name,v);} return {}; }
}

any EvalVisitor::visitAugassign(Python3Parser::AugassignContext *ctx){ return {}; }
any EvalVisitor::visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx){ if(ctx->return_stmt()) return visit(ctx->return_stmt()); if(ctx->break_stmt()) return visit(ctx->break_stmt()); return visit(ctx->continue_stmt()); }
any EvalVisitor::visitBreak_stmt(Python3Parser::Break_stmtContext *ctx){ throw BreakSignal(); }
any EvalVisitor::visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx){ throw ContinueSignal(); }
any EvalVisitor::visitReturn_stmt(Python3Parser::Return_stmtContext *ctx){ std::vector<Value> vals; if(ctx->testlist()) vals = evalTestlist(ctx->testlist()); throw ReturnSignal{vals}; }

any EvalVisitor::visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx){ if(ctx->if_stmt()) return visit(ctx->if_stmt()); if(ctx->while_stmt()) return visit(ctx->while_stmt()); if(ctx->funcdef()) return visit(ctx->funcdef()); return {}; }

any EvalVisitor::visitIf_stmt(Python3Parser::If_stmtContext *ctx){ size_t nTests = ctx->test().size(); for(size_t i=0;i<nTests;i++){ Value cond = evalTest(ctx->test(i)); if(truthy(cond)){ visit(ctx->suite(i)); return {}; } } if(ctx->ELSE()) visit(ctx->suite(nTests)); return {}; }

any EvalVisitor::visitWhile_stmt(Python3Parser::While_stmtContext *ctx){ while(truthy(evalTest(ctx->test()))){ try{ visit(ctx->suite()); }catch(const BreakSignal&){ break; }catch(const ContinueSignal&){ continue; }catch(const ReturnSignal& rs){ throw rs; } } return {}; }

any EvalVisitor::visitSuite(Python3Parser::SuiteContext *ctx){ if(ctx->simple_stmt()) return visit(ctx->simple_stmt()); for(auto st: ctx->stmt()) visit(st); return {}; }

Value EvalVisitor::evalTest(Python3Parser::TestContext *ctx){ return any_cast<Value>(visit(ctx->or_test())); }

any EvalVisitor::visitTest(Python3Parser::TestContext *ctx){ return visit(ctx->or_test()); }

any EvalVisitor::visitOr_test(Python3Parser::Or_testContext *ctx){ Value res = any_cast<Value>(visit(ctx->and_test(0))); for(size_t i=1;i<ctx->and_test().size();++i){ if(truthy(res)) return res; Value rhs = any_cast<Value>(visit(ctx->and_test(i))); res = Value::fromBool(truthy(res)||truthy(rhs)); } return res; }

any EvalVisitor::visitAnd_test(Python3Parser::And_testContext *ctx){ Value res = any_cast<Value>(visit(ctx->not_test(0))); for(size_t i=1;i<ctx->not_test().size();++i){ if(!truthy(res)) return res; Value rhs = any_cast<Value>(visit(ctx->not_test(i))); res = Value::fromBool(truthy(res)&&truthy(rhs)); } return res; }

any EvalVisitor::visitNot_test(Python3Parser::Not_testContext *ctx){ if(ctx->NOT()) { Value v = any_cast<Value>(visit(ctx->not_test())); return Value::fromBool(!truthy(v)); } return visit(ctx->comparison()); }

static int cmpStrings(const string &a,const string &b){ if(a<b) return -1; if(a>b) return 1; return 0; }

int EvalVisitor::cmpNumbers(const Value &a,const Value &b){ if(isStr(a)&&isStr(b)) return cmpStrings(a.s,b.s); if(isFloat(a)||isFloat(b)){ double da=toDouble(a), db=toDouble(b); if(da<db) return -1; if(da>db) return 1; return 0; } BigInt ia=toBig(a), ib=toBig(b); if(ia.neg!=ib.neg){ return ia.neg? -1: 1; } int cmp=ia.cmpAbs(ib); return ia.neg? -cmp : cmp; }

any EvalVisitor::visitComparison(Python3Parser::ComparisonContext *ctx){ Value left = any_cast<Value>(visit(ctx->arith_expr(0))); if(ctx->comp_op().empty()) return left; for(size_t i=0;i<ctx->comp_op().size();++i){ Value right = any_cast<Value>(visit(ctx->arith_expr(i+1))); string op = ctx->comp_op(i)->getText(); bool ok=false; int c = cmpNumbers(left,right);
	if(op=="<") ok = c<0; else if(op==">") ok = c>0; else if(op=="==") ok = c==0; else if(op==">=") ok = c>=0; else if(op=="<=") ok = c<=0; else if(op!="!=") ; else ok = c!=0; if(!ok) return Value::fromBool(false); left = right; }
	return Value::fromBool(true);
}

any EvalVisitor::visitComp_op(Python3Parser::Comp_opContext *ctx){ return {}; }

any EvalVisitor::visitArith_expr(Python3Parser::Arith_exprContext *ctx){ Value res = any_cast<Value>(visit(ctx->term(0))); for(size_t i=1;i<ctx->term().size();++i){ string op = ctx->addorsub_op(i-1)->getText(); Value rhs = any_cast<Value>(visit(ctx->term(i))); res = (op=="+")? numAdd(res,rhs) : numSub(res,rhs); } return res; }

any EvalVisitor::visitAddorsub_op(Python3Parser::Addorsub_opContext *ctx){ return {}; }

any EvalVisitor::visitTerm(Python3Parser::TermContext *ctx){ Value res = any_cast<Value>(visit(ctx->factor(0))); for(size_t i=1;i<ctx->factor().size();++i){ string op = ctx->muldivmod_op(i-1)->getText(); Value rhs = any_cast<Value>(visit(ctx->factor(i))); if(op=="*") res=numMul(res,rhs); else if(op=="/") res=numDiv(res,rhs); else if(op=="//") res=numIDiv(res,rhs); else res=numMod(res,rhs); } return res; }

any EvalVisitor::visitMuldivmod_op(Python3Parser::Muldivmod_opContext *ctx){ return {}; }

any EvalVisitor::visitFactor(Python3Parser::FactorContext *ctx){ if(ctx->atom_expr()) return visit(ctx->atom_expr()); Value v = any_cast<Value>(visit(ctx->factor())); if(ctx->ADD()) return v; BigInt zero=BigInt::fromLL(0); if(v.type==Value::TInt){ BigInt r = BigInt::sub(zero, v.i); return Value::fromInt(r); } else { return Value::fromFloat(-toDouble(v)); } }

Value EvalVisitor::evalAtomExpr(Python3Parser::Atom_exprContext *ctx){ Value base = evalAtom(ctx->atom()); if(ctx->trailer()){ if(ctx->atom()->NAME()) return evalCall(ctx->atom()->NAME()->getText(), ctx->trailer()->arglist()); return Value::None(); } return base; }

any EvalVisitor::visitAtom_expr(Python3Parser::Atom_exprContext *ctx){ return evalAtomExpr(ctx); }

any EvalVisitor::visitTrailer(Python3Parser::TrailerContext *ctx){ return {}; }

Value EvalVisitor::evalAtom(Python3Parser::AtomContext *ctx){ if(ctx->NAME()) return getVar(ctx->NAME()->getText()); if(ctx->NUMBER()){ string t=ctx->NUMBER()->getText(); if(t.find('.')!=string::npos){ return Value::fromFloat(std::stod(t)); } else { return Value::fromInt(BigInt::fromString(t)); } } if(ctx->TRUE()) return Value::fromBool(true); if(ctx->FALSE()) return Value::fromBool(false); if(ctx->NONE()) return Value::None(); if(ctx->OPEN_PAREN()) return evalTest(ctx->test()); if(ctx->STRING().size()>0){ string out=""; for(auto s: ctx->STRING()){
		string raw=s->getText(); if(raw.size()>=2) out += raw.substr(1, raw.size()-2);
		}
		return Value::fromStr(out);
	}
	if(ctx->format_string()) return any_cast<Value>(visit(ctx->format_string())); return Value::None(); }

any EvalVisitor::visitAtom(Python3Parser::AtomContext *ctx){ return evalAtom(ctx); }

any EvalVisitor::visitFormat_string(Python3Parser::Format_stringContext *ctx){ string result=""; size_t litIdx=0, exprIdx=0; for(size_t i=0;i<ctx->children.size();++i){ auto ch=ctx->children[i]; if(auto t = dynamic_cast<antlr4::tree::TerminalNode*>(ch)){ string txt=t->getText(); if(t->getSymbol()->getType()==Python3Parser::FORMAT_STRING_LITERAL){ // handle escapes {{ }}
			string frag=""; for(size_t j=0;j<txt.size();++j){ if(j+1<txt.size() && txt[j]=='{' && txt[j+1]=='{'){ frag += '{'; ++j; } else if(j+1<txt.size() && txt[j]=='}' && txt[j+1]=='}'){ frag += '}'; ++j; } else frag += txt[j]; }
			result += frag;
		}
	} else { // testlist enclosed by braces
		auto tl = ctx->testlist(exprIdx++); auto vals = evalTestlist(tl); // only first used
		Value v = vals[0]; if(v.type==Value::TFloat){ std::ostringstream os; os<<std::fixed<<std::setprecision(6)<<v.f; result+=os.str(); } else if(v.type==Value::TBool){ result += (v.b?"True":"False"); } else if(v.type==Value::TStr){ result += v.s; } else { result += toStr(v).s; }
	}
	}
	return Value::fromStr(result);
}

any EvalVisitor::visitTestlist(Python3Parser::TestlistContext *ctx){ return evalTestlist(ctx); }

vector<Value> EvalVisitor::evalTestlist(Python3Parser::TestlistContext *ctx){ vector<Value> vs; for(auto t: ctx->test()) vs.push_back(evalTest(t)); return vs; }
Value EvalVisitor::evalTestlistSingle(Python3Parser::TestlistContext *ctx){ auto vs=evalTestlist(ctx); return vs.size()==1?vs[0]:Value::fromTuple(vs); }

any EvalVisitor::visitArglist(Python3Parser::ArglistContext *ctx){ return {}; }
any EvalVisitor::visitArgument(Python3Parser::ArgumentContext *ctx){ return {}; }

any EvalVisitor::visitFuncdef(Python3Parser::FuncdefContext *ctx){ Value::Func f; f.body = ctx->suite(); // collect parameters
	if(ctx->parameters()->typedargslist()){
		auto tl = ctx->parameters()->typedargslist(); for(size_t i=0;i<tl->tfpdef().size();++i){ f.params.push_back(tl->tfpdef(i)->NAME()->getText()); bool hasDef = (i < tl->test().size()); f.hasDefault.push_back(hasDef); if(hasDef) f.defaults.push_back(tl->test(i)); else f.defaults.push_back(nullptr); }
	}
	globals[ctx->NAME()->getText()] = Value::fromFunc(f); return {}; }

any EvalVisitor::visitParameters(Python3Parser::ParametersContext *ctx){ return {}; }
any EvalVisitor::visitTypedargslist(Python3Parser::TypedargslistContext *ctx){ return {}; }
any EvalVisitor::visitTfpdef(Python3Parser::TfpdefContext *ctx){ return {}; }

Value EvalVisitor::evalCall(const string &name, Python3Parser::ArglistContext *args){
	if(name=="print"){ std::vector<Value> vs; if(args) for(auto a: args->argument()) vs.push_back(evalTest(a->test(0))); for(size_t i=0;i<vs.size();++i){ Value v=vs[i]; if(v.type==Value::TFloat){ std::cout<<std::fixed<<std::setprecision(6)<<v.f; } else if(v.type==Value::TStr){ std::cout<<v.s; } else if(v.type==Value::TBool){ std::cout<<(v.b?"True":"False"); } else if(v.type==Value::TInt){ std::cout<<v.i.toString(); } else { std::cout<<"None"; } if(i+1<vs.size()) std::cout<<" "; } std::cout<<"\n"; return Value::None(); }
	if(name=="int"){ Value v = evalTest(args->argument(0)->test(0)); return toInt(v); }
	if(name=="float"){ Value v = evalTest(args->argument(0)->test(0)); return toFloat(v); }
	if(name=="str"){ Value v = evalTest(args->argument(0)->test(0)); return toStr(v); }
	if(name=="bool"){ Value v = evalTest(args->argument(0)->test(0)); return toBool(v); }
	// user-defined
	Value fv = getVar(name); if(fv.type!=Value::TFunc) return Value::None(); Value::Func &fn = fv.func;
	std::unordered_map<std::string, Value> newLoc;
	// positional first, then keyword
	std::vector<bool> filled(fn.params.size(), false);
	if(args){ for(auto a: args->argument()){ if(a->ASSIGN()){ string key = a->test(0)->or_test()->and_test(0)->not_test(0)->comparison()->arith_expr(0)->term(0)->factor(0)->atom_expr()->atom()->NAME()->getText(); Value val = evalTest(a->test(1)); // find param index
			for(size_t i=0;i<fn.params.size();++i) if(fn.params[i]==key){ newLoc[key]=val; filled[i]=true; break; }
		} else { // positional
			for(size_t i=0;i<fn.params.size();++i) if(!filled[i]){ newLoc[fn.params[i]] = evalTest(a->test(0)); filled[i]=true; break; }
		}
	} }
	// defaults
	for(size_t i=0;i<fn.params.size();++i){ if(!filled[i]){ if(fn.hasDefault[i]) newLoc[fn.params[i]] = evalTest(fn.defaults[i]); else newLoc[fn.params[i]] = Value::None(); }}
	localsStack.push_back(std::move(newLoc));
	try{ visit(fn.body); localsStack.pop_back(); return Value::None(); } catch(const ReturnSignal &rs){ localsStack.pop_back(); if(rs.values.empty()) return Value::None(); if(rs.values.size()==1) return rs.values[0]; return Value::fromTuple(rs.values); }
}
