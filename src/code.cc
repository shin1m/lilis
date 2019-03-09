#include "code.h"

namespace lilis
{

namespace
{

template<typename T>
struct t_with_expression : t_with_value<t_object_of<t_with_expression<T>>, T>
{
	t_object* v_expression;

	t_with_expression(T* a_value, t_object* a_expression) : t_with_value<t_object_of<t_with_expression<T>>, T>(a_value), v_expression(a_expression)
	{
	}
	virtual void f_scan(gc::t_collector& a_collector)
	{
		t_with_value<t_object_of<t_with_expression<T>>, T>::f_scan(a_collector);
		v_expression = a_collector.f_forward(v_expression);
	}
};

}

t_object* t_module::t_variable::f_generate(t_code* a_code, t_object* a_expression)
{
	struct t_set : t_with_expression<t_variable>
	{
		using t_with_expression<t_variable>::t_with_expression;
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
		{
			a_emit(e_instruction__PUSH, a_stack + 1)(this);
			v_expression->f_emit(a_emit, a_stack + 1, false);
			a_emit(a_tail ? e_instruction__CALL_TAIL : e_instruction__CALL, a_stack + 1)(1);
		}
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			a_engine.v_used[-1] = v_value->v_value = *--a_engine.v_used;
		}
	};
	auto& engine = a_code->v_engine;
	return engine.f_new<t_set>(engine.f_pointer(this), engine.f_pointer(a_expression));
}

void t_module::t_variable::f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
{
	a_emit(e_instruction__PUSH, a_stack + 1)(this);
	a_emit(a_tail ? e_instruction__CALL_TAIL : e_instruction__CALL, a_stack + 1)(size_t(0));
}

void t_module::t_variable::f_call(t_engine& a_engine, size_t a_arguments)
{
	a_engine.v_used[-1] = v_value;
}

size_t t_code::t_local::f_outer(t_code* a_code) const
{
	for (size_t i = 0;; ++i) {
		if (a_code == *v_value) return i;
		if (!a_code->v_outer) throw std::runtime_error("out of scope");
		a_code = *a_code->v_outer;
	}
}

t_object* t_code::t_local::f_generate(t_code* a_code, t_object* a_expression)
{
	struct t_set : t_with_expression<t_local>
	{
		using t_with_expression<t_local>::t_with_expression;
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
		{
			v_expression->f_emit(a_emit, a_stack, false);
			a_emit(e_instruction__SET, a_stack + 1)(v_value->f_outer(a_emit.v_code))(v_value->v_index);
		}
	};
	auto& engine = a_code->v_engine;
	return engine.f_new<t_set>(engine.f_pointer(this), engine.f_pointer(a_expression));
}

void t_code::t_local::f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
{
	a_emit(e_instruction__GET, a_stack + 1)(f_outer(a_emit.v_code))(v_index);
}

void t_code::f_scan()
{
	v_this = v_engine.f_forward(v_this);
	v_outer = v_engine.f_forward(v_outer);
	v_module = v_engine.f_forward(v_module);
	for (auto& x : v_imports) x = v_engine.f_forward(x);
	for (auto& x : v_locals) x = v_engine.f_forward(x);
	v_bindings.f_scan(v_engine);
	for (auto i : v_objects) v_instructions[i] = v_engine.f_forward(static_cast<t_object*>(v_instructions[i]));
}

t_object* t_code::f_resolve(t_symbol* a_symbol) const
{
	for (auto code = this;; code = *code->v_outer) {
		if (auto p = code->v_bindings.f_find(a_symbol)) return p;
		for (auto i = code->v_imports.rbegin(); i != code->v_imports.rend(); ++i)
			if (auto p = (**i)->f_find(a_symbol)) return p;
		if (!code->v_outer) throw std::runtime_error("not found");
	}
}

void t_code::f_compile(t_pair* a_body)
{
	t_emit emit{this};
	if (a_body) {
		auto body = v_engine.f_pointer(a_body);
		for (; body->v_tail; body = f_cast<t_pair>(body->v_tail)) {
			f_generate(body->v_head)->f_emit(emit, 0, false);
			emit(e_instruction__POP, 0);
		}
		f_generate(body->v_head)->f_emit(emit, 0, true);
	} else {
		emit(e_instruction__PUSH, 1)(static_cast<t_object*>(nullptr));
	}
	emit(e_instruction__RETURN, 0);
	for (auto& x : emit.v_labels) {
		auto p = v_instructions.data() + x.v_target;
		for (auto i : x) v_instructions[i] = p;
	}
}

t_holder<t_code>* t_code::f_new(t_object* a_body)
{
	auto body = v_engine.f_pointer(a_body);
	auto code = v_engine.f_pointer(v_engine.f_new<t_holder<t_code>>(v_engine, v_this, nullptr));
	for (auto arguments = v_engine.f_pointer(f_chop(body)); arguments;) {
		auto symbol = v_engine.f_pointer(dynamic_cast<t_symbol*>(static_cast<t_object*>(arguments)));
		if (symbol) {
			(*code)->v_rest = true;
			arguments = nullptr;
		} else {
			symbol = f_cast<t_symbol>(f_chop(arguments));
			++(*code)->v_arguments;
		}
		(*code)->v_bindings.emplace(symbol, v_engine.f_new<t_local>(code, (*code)->v_locals.size()));
		(*code)->v_locals.push_back(symbol);
	}
	(*code)->f_compile(body ? f_cast<t_pair>(static_cast<t_object*>(body)) : nullptr);
	return code;
}

size_t t_scope::f_size() const
{
	return sizeof(t_scope) + sizeof(t_object*) * v_size;
}

void t_scope::f_scan(gc::t_collector& a_collector)
{
	v_outer = a_collector.f_forward(v_outer);
	auto p = f_locals();
	for (size_t i = 0; i < v_size; ++i) p[i] = a_collector.f_forward(p[i]);
}

void t_call::f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
{
	v_value->v_head->f_emit(a_emit, a_stack, false);
	auto n = a_stack;
	for (auto p = static_cast<t_pair*>(v_value->v_tail); p; p = static_cast<t_pair*>(p->v_tail)) p->v_head->f_emit(a_emit, ++n, false);
	int instruction = v_expand ? e_instruction__CALL_WITH_EXPANSION : e_instruction__CALL;
	if (a_tail) instruction += e_instruction__CALL_TAIL - e_instruction__CALL;
	a_emit(static_cast<t_instruction>(instruction), a_stack + 1)(n - a_stack);
}

}
