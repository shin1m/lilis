#include "code.h"
#include "builtins.h"

namespace lilis
{

t_object* t_static::f_forward(gc::t_collector& a_collector)
{
	return this;
}

namespace
{

struct : t_static
{
	struct t_instantiate : t_with_value<t_object_of<t_instantiate>, t_holder<t_code>>
	{
		using t_base::t_base;
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
		{
			a_emit((*v_value)->v_rest ? e_instruction__LAMBDA_WITH_REST : e_instruction__LAMBDA, a_stack + 1)(v_value);
		}
	};

	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
	{
		auto& engine = a_code->v_engine;
		return engine.f_new<t_instantiate>(engine.f_pointer(a_code->f_new(a_arguments)));
	}
} v_lambda;

struct : t_static
{
	struct t_instance : t_with_value<t_object_of<t_instance>, t_pair>
	{
		using t_base::t_base;
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
		{
			auto body = a_emit.v_code->v_engine.f_pointer(v_value);
			for (; body->v_tail; body = f_cast<t_pair>(body->v_tail)) {
				body->v_head->f_emit(a_emit, a_stack, false);
				a_emit(e_instruction__POP, a_stack);
			}
			body->v_head->f_emit(a_emit, a_stack, a_tail);
		}
	};

	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
	{
		auto& engine = a_code->v_engine;
		if (!a_arguments) return engine.f_new<t_quote>(nullptr);
		auto body = engine.f_pointer(a_arguments);
		auto last = engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(a_code->f_generate(f_chop(body))), nullptr));
		auto block = engine.f_pointer(engine.f_new<t_instance>(last));
		while (body) f_push(engine, last, a_code->f_generate(f_chop(body)));
		return block;
	}
} v_begin;

}

t_object* t_define::f_apply(t_code* a_code, t_object* a_arguments)
{
	auto& engine = a_code->v_engine;
	auto arguments = engine.f_pointer(a_arguments);
	auto symbol = engine.f_pointer(f_cast<t_symbol>(f_chop(arguments)));
	auto expression = engine.f_pointer(f_chop(arguments));
	if (arguments) throw std::runtime_error("must be nil");
	auto local = engine.f_pointer(engine.f_new<t_code::t_local>(a_code->v_this, a_code->v_locals.size()));
	a_code->v_bindings.emplace(symbol, local);
	a_code->v_locals.push_back(symbol);
	auto value = a_code->f_generate(expression);
	return local->f_generate(a_code, value);
}

t_define v_define;

t_object* t_set::f_apply(t_code* a_code, t_object* a_arguments)
{
	auto& engine = a_code->v_engine;
	auto arguments = engine.f_pointer(a_arguments);
	auto bound = engine.f_pointer(a_code->f_generate(f_chop(arguments)));
	auto value = a_code->f_generate(f_chop(arguments));
	if (arguments) throw std::runtime_error("must be nil");
	if (auto p = dynamic_cast<t_mutable*>(static_cast<t_object*>(bound))) return p->f_generate(a_code, value);
	throw std::runtime_error("not mutable");
}

t_set v_set;

t_object* t_macro::f_apply(t_code* a_code, t_object* a_arguments)
{
	struct t_instance : t_with_value<t_object_of<t_instance>, t_holder<t_code>>
	{
		using t_base::t_base;
		virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
		{
			auto& engine = a_code->v_engine;
			engine.f_run(*v_value, a_arguments);
			return a_code->f_generate(engine.v_used[0]);
		}
	};
	auto& engine = a_code->v_engine;
	auto arguments = engine.f_pointer(a_arguments);
	auto symbol = engine.f_pointer(f_cast<t_symbol>(f_chop(arguments)));
	return a_code->v_bindings.insert_or_assign(symbol, engine.f_new<t_instance>(engine.f_pointer(a_code->f_new(arguments)))).first->second;
}

t_macro v_macro;

namespace
{

struct : t_static
{
	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
	{
		auto& engine = a_code->v_engine;
		auto arguments = engine.f_pointer(a_arguments);
		auto symbol = engine.f_pointer(f_cast<t_symbol>(f_chop(arguments)));
		if (arguments) throw std::runtime_error("must be nil");
		auto bound = engine.f_pointer(a_code->f_resolve(symbol));
		while (!a_code->v_module) a_code = *a_code->v_outer;
		if (dynamic_cast<t_mutable*>(static_cast<t_object*>(bound))) {
			auto variable = engine.f_pointer(engine.f_new<t_module::t_variable>(nullptr));
			(*a_code->v_module)->insert_or_assign(symbol, variable);
			return variable->f_generate(a_code, bound);
		}
		(*a_code->v_module)->insert_or_assign(symbol, bound);
		return engine.f_new<t_quote>(nullptr);
	}
} v_export;

struct : t_static
{
	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
	{
		auto& engine = a_code->v_engine;
		auto arguments = engine.f_pointer(a_arguments);
		auto symbol = f_cast<t_symbol>(f_chop(arguments));
		if (arguments) throw std::runtime_error("must be nil");
		auto code = a_code;
		while (!code->v_module) code = *code->v_outer;
		a_code->v_imports.push_back(engine.f_module((*code->v_module)->v_path.parent_path(), symbol->v_entry->first));
		return engine.f_new<t_quote>(nullptr);
	}
} v_import;

struct : t_static
{
	struct t_instance : t_object_of<t_instance>
	{
		t_object* v_condition;
		t_object* v_true;
		t_object* v_false;

		t_instance(t_object* a_condition, t_object* a_true, t_object* a_false) : v_condition(a_condition), v_true(a_true), v_false(a_false)
		{
		}
		virtual void f_scan(gc::t_collector& a_collector)
		{
			v_condition = a_collector.f_forward(v_condition);
			v_true = a_collector.f_forward(v_true);
			v_false = a_collector.f_forward(v_false);
		}
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
		{
			v_condition->f_emit(a_emit, a_stack, false);
			auto& label0 = a_emit.v_labels.emplace_back();
			a_emit(e_instruction__BRANCH, a_stack)(label0);
			v_true->f_emit(a_emit, a_stack, a_tail);
			auto& label1 = a_emit.v_labels.emplace_back();
			a_emit(e_instruction__JUMP, a_stack)(label1);
			label0.v_target = a_emit.v_code->v_instructions.size();
			if (v_false)
				v_false->f_emit(a_emit, a_stack, a_tail);
			else
				a_emit(e_instruction__PUSH, a_stack + 1)(static_cast<t_object*>(nullptr));
			label1.v_target = a_emit.v_code->v_instructions.size();
		}
	};

	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
	{
		auto& engine = a_code->v_engine;
		auto arguments = engine.f_pointer(a_arguments);
		auto condition = engine.f_pointer(a_code->f_generate(f_chop(arguments)));
		auto truee = engine.f_pointer(a_code->f_generate(f_chop(arguments)));
		if (!arguments) return engine.f_new<t_instance>(condition, truee, nullptr);
		auto falsee = engine.f_pointer(a_code->f_generate(f_chop(arguments)));
		if (arguments) throw std::runtime_error("must be nil");
		return engine.f_new<t_instance>(condition, truee, falsee);
	}
} v_if;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 2) throw std::runtime_error("requires two arguments");
		a_engine.v_used -= 2;
		a_engine.v_used[-1] = a_engine.v_used[0] == a_engine.v_used[1] ? this : nullptr;
	}
} v_eq;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 1) throw std::runtime_error("requires an argument");
		--a_engine.v_used;
		a_engine.v_used[-1] = dynamic_cast<t_pair*>(a_engine.v_used[0]);
	}
} v_is_pair;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 2) throw std::runtime_error("requires two arguments");
		auto used = a_engine.v_used - 2;
		used[-1] = a_engine.f_new<t_pair>(used[0], used[1]);
		a_engine.v_used = used;
	}
} v_cons;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 1) throw std::runtime_error("requires an argument");
		--a_engine.v_used;
		a_engine.v_used[-1] = f_cast<t_pair>(a_engine.v_used[0])->v_head;
	}
} v_car;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 1) throw std::runtime_error("requires an argument");
		--a_engine.v_used;
		a_engine.v_used[-1] = f_cast<t_pair>(a_engine.v_used[0])->v_tail;
	}
} v_cdr;

struct : t_static
{
	struct t_instance : t_symbol
	{
		t_instance() : t_symbol({})
		{
		}
		virtual void f_scan(gc::t_collector& a_collector)
		{
		}
		virtual void f_destruct(gc::t_collector& a_collector)
		{
		}
		virtual std::wstring f_string() const
		{
			return t_object::f_string();
		}
	};

	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments > 0) throw std::runtime_error("requires no arguments");
		a_engine.v_used[-1] = a_engine.f_new<t_instance>();
	}
} v_gensym;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		a_engine.v_used -= a_arguments;
		if (a_arguments > 0)
			for (size_t i = 0;;) {
				std::wcout << lilis::f_string(a_engine.v_used[i]);
				if (++i >= a_arguments) break;
				std::wcout << L' ';
			}
		std::wcout << std::endl;
		a_engine.v_used[-1] = nullptr;
	}
} v_print;

namespace prompt
{
	struct t_continuation : t_object_of<t_continuation>
	{
		size_t v_stack;
		size_t v_contexts;

		t_continuation(t_context* a_context, size_t a_stack, size_t a_contexts) : v_stack(a_stack), v_contexts(a_contexts)
		{
			auto head = a_context[v_contexts - 1].v_stack;
			auto contexts = reinterpret_cast<t_context*>(std::copy_n(head, v_stack, reinterpret_cast<t_object**>(this + 1)));
			std::copy_n(a_context, v_contexts, contexts);
			for (size_t i = 0; i < v_contexts; ++i) contexts[i].v_stack -= head - static_cast<t_object**>(nullptr);
		}
		virtual size_t f_size() const
		{
			return sizeof(t_continuation) + sizeof(t_object*) * v_stack + sizeof(t_context) * v_contexts;
		}
		virtual void f_scan(gc::t_collector& a_collector)
		{
			auto p = reinterpret_cast<t_object**>(this + 1);
			for (size_t i = 0; i < v_stack; ++i, ++p) *p = a_collector.f_forward(*p);
			auto q = reinterpret_cast<t_context*>(p);
			for (size_t i = 0; i < v_contexts; ++i, ++q) q->f_scan(a_collector);
		}
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments != 1) throw std::runtime_error("requires an argument");
			auto used = a_engine.v_used - 2;
			auto value = used[1];
			auto p = reinterpret_cast<t_object**>(this + 1);
			a_engine.v_used = std::copy_n(p, v_stack, used);
			a_engine.v_context -= v_contexts;
			std::copy_n(reinterpret_cast<t_context*>(p + v_stack), v_contexts, a_engine.v_context);
			for (size_t i = 0; i < v_contexts; ++i) a_engine.v_context[i].v_stack += used - static_cast<t_object**>(nullptr);
			*a_engine.v_used++ = value;
		}
	};
	struct t_call : t_static
	{
		inline static void* v_return = reinterpret_cast<void*>(e_instruction__RETURN);

		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments != 3) throw std::runtime_error("requires three arguments");
			if (a_engine.v_context <= a_engine.v_contexts.get()) throw std::runtime_error("stack overflow");
			--a_engine.v_context;
			a_engine.v_context->v_stack = a_engine.v_used - 4;
			a_engine.v_context->v_code = nullptr;
			a_engine.v_context->v_current = &v_return;
			a_engine.v_context->v_scope = nullptr;
			a_engine.v_used[-1]->f_call(a_engine, 0);
		}
	} v_call;
	struct : t_static
	{
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments < 1) throw std::runtime_error("requires at least one argument");
			auto tail = a_engine.v_used - a_arguments - 1;
			auto context = a_engine.v_context;
			while (!dynamic_cast<t_call*>(context->v_stack[0]) || context->v_stack[1] != tail[1])
				if (++context == a_engine.v_contexts.get() + t_engine::V_CONTEXTS)
					throw std::runtime_error("no matching prompt found");
			auto head = context->v_stack;
			auto stack = tail - head;
			auto contexts = ++context - a_engine.v_context;
			head[1] = new(a_engine.f_allocate(sizeof(t_continuation) + sizeof(t_object*) * stack + sizeof(t_context) * contexts)) t_continuation(a_engine.v_context, stack, contexts);
			head[0] = this;
			auto handler = head[2];
			a_engine.v_used = std::copy(tail + 2, a_engine.v_used, head + 2);
			a_engine.v_context = context;
			handler->f_call(a_engine, a_arguments);
		}
	} v_abort;
}

struct : t_static
{
	static t_object* f_append(t_engine& a_engine, t_object* a_list, t_object* a_tail)
	{
		if (!a_list) return a_tail;
		auto tail = a_engine.f_pointer(a_tail);
		auto pair = a_engine.f_pointer(f_cast<t_pair>(a_list));
		auto list = a_engine.f_pointer(a_engine.f_new<t_pair>(a_engine.f_pointer(pair->v_head), nullptr));
		auto last = a_engine.f_pointer<t_pair>(list);
		while (pair->v_tail) {
			pair = f_cast<t_pair>(pair->v_tail);
			f_push(a_engine, last, pair->v_head);
		}
		last->v_tail = tail;
		return list;
	}

	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 2) throw std::runtime_error("requires two arguments");
		auto used = a_engine.v_used - 2;
		used[-1] = f_append(a_engine, used[0], used[1]);
		a_engine.v_used = used;
	}
} v_append;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 1) throw std::runtime_error("requires an argument");
		a_engine.v_used[-2] = a_engine.f_new<t_quote>(a_engine.v_used[-1]);
		--a_engine.v_used;
	}
} v_quote;

}

t_object* f_unquasiquote(t_code* a_code, t_object* a_value)
{
	auto& engine = a_code->v_engine;
	if (auto p = dynamic_cast<t_pair*>(a_value)) {
		auto pair = engine.f_pointer(p);
		auto tail = engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(f_unquasiquote(a_code, pair->v_tail)), nullptr));
		if (auto p = dynamic_cast<t_unquote_splicing*>(pair->v_head))
			return engine.f_new<t_call>(engine.f_pointer(engine.f_new<t_pair>(&v_append,
				engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(a_code->f_generate(p->v_value)), tail))
			)));
		else
			return engine.f_new<t_call>(engine.f_pointer(engine.f_new<t_pair>(&v_cons,
				engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(f_unquasiquote(a_code, pair->v_head)), tail))
			)));
	}
	if (auto p = dynamic_cast<t_quote*>(a_value))
		return engine.f_new<t_call>(engine.f_pointer(engine.f_new<t_pair>(&v_quote,
			engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(f_unquasiquote(a_code, p->v_value)), nullptr))
		)));
	if (auto p = dynamic_cast<t_unquote*>(a_value)) return a_code->f_generate(p->v_value);
	return engine.f_new<t_quote>(engine.f_pointer(a_value));
}

void f_define_builtins(t_module& a_module)
{
	a_module.f_register(L"lambda"sv, &v_lambda);
	a_module.f_register(L"begin"sv, &v_begin);
	a_module.f_register(L"define"sv, &v_define);
	a_module.f_register(L"set!"sv, &v_set);
	a_module.f_register(L"define-macro"sv, &v_macro);
	a_module.f_register(L"export"sv, &v_export);
	a_module.f_register(L"import"sv, &v_import);
	a_module.f_register(L"if"sv, &v_if);
	a_module.f_register(L"eq?"sv, &v_eq);
	a_module.f_register(L"pair?"sv, &v_is_pair);
	a_module.f_register(L"cons"sv, &v_cons);
	a_module.f_register(L"car"sv, &v_car);
	a_module.f_register(L"cdr"sv, &v_cdr);
	a_module.f_register(L"gensym"sv, &v_gensym);
	a_module.f_register(L"print"sv, &v_print);
	a_module.f_register(L"call-with-prompt"sv, &prompt::v_call);
	a_module.f_register(L"abort-to-prompt"sv, &prompt::v_abort);
}

}
