#include "code.h"
#include "builtins.h"

namespace lilis
{

t_object* t_object::f_apply(t_code* a_code, t_object* a_arguments)
{
	auto& engine = a_code->v_engine;
	auto arguments = engine.f_pointer(a_arguments);
	auto last = engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(this), nullptr));
	auto call = engine.f_pointer(engine.f_new<t_call>(last));
	while (auto pair = dynamic_cast<t_pair*>(static_cast<t_object*>(arguments))) {
		arguments = pair->v_tail;
		f_push(engine, last, a_code->f_generate(pair->v_head));
	}
	if (arguments) {
		f_push(engine, last, arguments->f_generate(a_code));
		call->v_expand = true;
	}
	return call;
}

void t_object::f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
{
	a_emit(e_instruction__PUSH, a_stack + 1)(this);
}

void t_symbol::f_scan(gc::t_collector& a_collector)
{
	v_entry->second = this;
}

void t_symbol::f_destruct(gc::t_collector& a_collector)
{
	static_cast<t_engine&>(a_collector).v_symbols.erase(v_entry);
}

t_object* t_symbol::f_generate(t_code* a_code)
{
	return a_code->f_resolve(this);
}

std::wstring t_symbol::f_string() const
{
	return v_entry->first;
}

void t_pair::f_scan(gc::t_collector& a_collector)
{
	v_head = a_collector.f_forward(v_head);
	v_tail = a_collector.f_forward(v_tail);
}

t_object* t_pair::f_generate(t_code* a_code)
{
	auto tail = a_code->v_engine.f_pointer(v_tail);
	return a_code->f_generate(v_head)->f_apply(a_code, tail);
}

std::wstring t_pair::f_string() const
{
	auto s = L"("s;
	for (auto p = this;;) {
		s += lilis::f_string(p->v_head);
		if (!p->v_tail) break;
		auto tail = dynamic_cast<t_pair*>(p->v_tail);
		if (!tail) {
			s += L" . " + p->v_tail->f_string();
			break;
		}
		s += L' ';
		p = tail;
	}
	return s + L')';
}

void t_quote::f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
{
	a_emit(e_instruction__PUSH, a_stack + 1)(v_value);
}

std::wstring t_quote::f_string() const
{
	return L'\'' + lilis::f_string(v_value);
}

std::wstring t_unquote::f_string() const
{
	return L',' + lilis::f_string(v_value);
}

std::wstring t_unquote_splicing::f_string() const
{
	return L",@" + lilis::f_string(v_value);
}

t_object* t_quasiquote::f_generate(t_code* a_code)
{
	return f_unquasiquote(a_code, v_value);
}

std::wstring t_quasiquote::f_string() const
{
	return L'`' + lilis::f_string(v_value);
}

}
