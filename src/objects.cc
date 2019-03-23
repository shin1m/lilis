#include "code.h"
#include "builtins.h"

namespace lilis
{

t_object* t_object::f_render(t_code& a_code, const t_location& a_location)
{
	return this;
}

t_object* t_object::f_apply(t_code& a_code, const t_location& a_location, t_pair* a_pair)
{
	auto& engine = a_code.v_engine;
	auto arguments = engine.f_pointer(a_pair->v_tail);
	auto location = a_location.f_at_tail(a_pair);
	auto last = engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(this), nullptr));
	auto call = engine.f_pointer(engine.f_new<t_call>(last));
	while (auto p = dynamic_cast<t_pair*>(arguments.v_value)) {
		arguments = p->v_tail;
		location = a_location.f_at_tail(p);
		f_push(engine, last, a_code.f_render(p->v_head, a_location.f_at_head(p)));
	}
	if (arguments) {
		f_push(engine, last, a_code.f_render(arguments, location));
		call->v_expand = true;
	}
	return call;
}

void t_object::f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
{
	a_emit(e_instruction__PUSH, a_stack + 1)(this);
}

void t_object::f_call(t_engine& a_engine, size_t a_arguments)
{
	throw std::runtime_error("not callable");
}

std::wstring t_object::f_string() const
{
	return L"#object";
}

void t_symbol::f_scan(gc::t_collector& a_collector)
{
	v_entry->second = this;
}

void t_symbol::f_destruct(gc::t_collector& a_collector)
{
	static_cast<t_engine&>(a_collector).v_symbols.erase(v_entry);
}

t_object* t_symbol::f_render(t_code& a_code, const t_location& a_location)
{
	return a_code.f_resolve(this, a_location);
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

t_object* t_pair::f_render(t_code& a_code, const t_location& a_location)
{
	auto thiz = a_code.v_engine.f_pointer(this);
	return a_code.f_render(v_head, a_location)->f_apply(a_code, a_location, thiz);
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

t_object* t_quasiquote::f_render(t_code& a_code, const t_location& a_location)
{
	return f_unquasiquote(a_code, v_value);
}

std::wstring t_quasiquote::f_string() const
{
	return L'`' + lilis::f_string(v_value);
}

}
