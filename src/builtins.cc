#include "code.h"
#include "builtins.h"
#include "parser.h"

namespace lilis
{

namespace
{

struct t_static : t_object
{
	virtual t_object* f_forward(gc::t_collector& a_collector)
	{
		return this;
	}
};

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

	virtual t_object* f_apply(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_pair* a_pair)
	{
		auto& engine = a_code.v_engine;
		auto pair = engine.f_pointer(a_pair);
		auto code = engine.f_pointer(a_code.f_new());
		(*code)->f_compile(a_location, pair);
		return engine.f_new<t_instantiate>(code);
	}
} v_lambda;

struct : t_static
{
	struct t_instance : t_with_value<t_object_of<t_instance>, t_pair>
	{
		using t_base::t_base;
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
		{
			auto body = v_value;
			for (; body->v_tail; body = static_cast<t_pair*>(body->v_tail)) {
				body->v_head->f_emit(a_emit, a_stack, false);
				a_emit(e_instruction__POP, a_stack);
			}
			body->v_head->f_emit(a_emit, a_stack, a_tail);
		}
	};

	virtual t_object* f_apply(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_pair* a_pair)
	{
		auto& engine = a_code.v_engine;
		if (!a_pair->v_tail) return engine.f_new<t_quote>(nullptr);
		auto arguments = engine.f_pointer(a_location->f_cast_tail<t_pair>(a_pair));
		auto last = engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(a_code.f_render(arguments->v_head, a_location->f_at_head(arguments))), nullptr));
		auto block = engine.f_pointer(engine.f_new<t_instance>(last));
		while (arguments->v_tail) {
			arguments = a_location->f_cast_tail<t_pair>(arguments);
			f_push(engine, last, a_code.f_render(arguments->v_head, a_location->f_at_head(arguments)));
		}
		return block;
	}
} v_begin;

struct : t_static
{
	virtual t_object* f_apply(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_pair* a_pair)
	{
		auto& engine = a_code.v_engine;
		auto arguments = engine.f_pointer(a_location->f_cast_tail<t_pair>(a_pair));
		auto symbol = engine.f_pointer(a_location->f_cast_head<t_symbol>(arguments));
		arguments = a_location->f_cast_tail<t_pair>(arguments);
		auto expression = engine.f_pointer(arguments->v_head);
		a_location->f_nil_tail(arguments);
		auto local = engine.f_pointer(engine.f_new<t_code::t_local>(a_code.v_this, a_code.v_locals.size()));
		a_code.v_bindings.emplace(symbol, local);
		a_code.v_locals.push_back(symbol);
		auto value = a_code.f_render(expression, a_location->f_at_head(arguments));
		return local->f_render(a_code, value);
	}
} v_define;

struct : t_static
{
	virtual t_object* f_apply(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_pair* a_pair)
	{
		auto& engine = a_code.v_engine;
		auto arguments = engine.f_pointer(a_location->f_cast_tail<t_pair>(a_pair));
		auto location = a_location->f_at_head(arguments);
		auto bound = engine.f_pointer(a_code.f_render(arguments->v_head, location));
		location->f_try([&]
		{
			if (!dynamic_cast<t_mutable*>(bound.v_value)) throw t_error{L"not mutable"s};
		});
		arguments = a_location->f_cast_tail<t_pair>(arguments);
		auto value = a_code.f_render(arguments->v_head, a_location->f_at_head(arguments));
		a_location->f_nil_tail(arguments);
		return dynamic_cast<t_mutable*>(bound.v_value)->f_render(a_code, value);
	}
} v_set;

struct : t_static
{
	struct t_instance : t_with_value<t_object_of<t_instance>, t_holder<t_code>>
	{
		using t_base::t_base;
		virtual t_object* f_apply(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_pair* a_pair)
		{
			return a_location->f_try([&]
			{
				auto& engine = a_code.v_engine;
				engine.f_run(*v_value, a_pair->v_tail);
				auto p = engine.v_used[0];
				return a_code.f_render(p, std::make_shared<t_at_expression>(engine, p));
			});
		}
	};

	virtual t_object* f_apply(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_pair* a_pair)
	{
		auto& engine = a_code.v_engine;
		auto at_tail = a_location->f_at_tail(a_pair);
		auto arguments = engine.f_pointer(at_tail->f_cast<t_pair>(a_pair->v_tail));
		auto symbol = engine.f_pointer(a_location->f_cast_head<t_symbol>(arguments));
		auto code = engine.f_pointer(a_code.f_new());
		(*code)->v_macro = true;
		(*code)->f_compile(at_tail, arguments);
		return a_code.v_bindings.insert_or_assign(symbol, engine.f_new<t_instance>(code)).first->second;
	}
} v_macro;

struct : t_static
{
	virtual t_object* f_apply(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_pair* a_pair)
	{
		auto& engine = a_code.v_engine;
		auto arguments = engine.f_pointer(a_location->f_cast_tail<t_pair>(a_pair));
		auto at_head = a_location->f_at_head(arguments);
		auto symbol = engine.f_pointer(at_head->f_cast<t_symbol>(arguments->v_head));
		auto bound = engine.f_pointer(a_code.f_resolve(symbol, at_head));
		a_location->f_nil_tail(arguments);
		if (dynamic_cast<t_mutable*>(bound.v_value)) {
			auto variable = engine.f_pointer(engine.f_new<t_module::t_variable>(nullptr));
			(*a_code.v_module)->insert_or_assign(symbol, variable);
			return variable->f_render(a_code, bound);
		}
		(*a_code.v_module)->insert_or_assign(symbol, bound);
		return engine.f_new<t_quote>(nullptr);
	}
} v_export;

struct : t_static
{
	virtual t_object* f_apply(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_pair* a_pair)
	{
		auto& engine = a_code.v_engine;
		auto arguments = engine.f_pointer(a_location->f_cast_tail<t_pair>(a_pair));
		auto location = a_location->f_at_head(arguments);
		a_code.v_imports.push_back(location->f_try([&]
		{
			return engine.f_module((*a_code.v_module)->v_path.parent_path(), location->f_cast<t_symbol>(arguments->v_head)->v_entry->first);
		}));
		a_location->f_nil_tail(arguments);
		return engine.f_new<t_quote>(nullptr);
	}
} v_import;

struct : t_static
{
	struct t_instance : t_object_of<t_instance>
	{
		t_object* v_condition;
		t_object* v_then;
		t_object* v_else;

		t_instance(t_object* a_condition, t_object* a_then, t_object* a_else) : v_condition(a_condition), v_then(a_then), v_else(a_else)
		{
		}
		virtual void f_scan(gc::t_collector& a_collector)
		{
			v_condition = a_collector.f_forward(v_condition);
			v_then = a_collector.f_forward(v_then);
			v_else = a_collector.f_forward(v_else);
		}
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
		{
			v_condition->f_emit(a_emit, a_stack, false);
			auto& label0 = a_emit.v_labels.emplace_back();
			a_emit(e_instruction__BRANCH, a_stack)(label0);
			v_then->f_emit(a_emit, a_stack, a_tail);
			auto& label1 = a_emit.v_labels.emplace_back();
			a_emit(e_instruction__JUMP, a_stack)(label1);
			label0.v_target = a_emit.v_code->v_instructions.size();
			if (v_else)
				v_else->f_emit(a_emit, a_stack, a_tail);
			else
				a_emit(e_instruction__PUSH, a_stack + 1)(static_cast<t_object*>(nullptr));
			label1.v_target = a_emit.v_code->v_instructions.size();
		}
	};

	virtual t_object* f_apply(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_pair* a_pair)
	{
		auto& engine = a_code.v_engine;
		auto arguments = engine.f_pointer(a_location->f_cast_tail<t_pair>(a_pair));
		auto condition = engine.f_pointer(a_code.f_render(arguments->v_head, a_location->f_at_head(arguments)));
		arguments = a_location->f_cast_tail<t_pair>(arguments);
		auto then = engine.f_pointer(a_code.f_render(arguments->v_head, a_location->f_at_head(arguments)));
		if (!arguments->v_tail) return engine.f_new<t_instance>(condition, then, nullptr);
		arguments = a_location->f_cast_tail<t_pair>(arguments);
		auto elsee = engine.f_pointer(a_code.f_render(arguments->v_head, a_location->f_at_head(arguments)));
		a_location->f_nil_tail(arguments);
		return engine.f_new<t_instance>(condition, then, elsee);
	}
} v_if;

template<typename T>
inline void f_try(t_engine& a_engine, size_t a_arguments, T a_do)
{
	auto used = a_engine.v_used - a_arguments;
	try {
		a_do(used);
		a_engine.v_used = used;
	} catch (...) {
		a_engine.v_used = used - 1;
		throw;
	}
}

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments != 2) throw t_error{L"requires OBJECT OBJECT"s};
			a_xs[-1] = a_xs[0] == a_xs[1] ? this : nullptr;
		});
	}
} v_eq;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments != 1) throw t_error{L"requires OBJECT"s};
			a_xs[-1] = dynamic_cast<t_pair*>(a_xs[0]);
		});
	}
} v_is_pair;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments != 2) throw t_error{L"requires OBJECT OBJECT"s};
			a_xs[-1] = a_engine.f_new<t_pair>(a_xs[0], a_xs[1]);
		});
	}
} v_cons;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments != 1) throw t_error{L"requires PAIR"s};
			a_xs[-1] = f_cast<t_pair>(a_xs[0])->v_head;
		});
	}
} v_car;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments != 1) throw t_error{L"requires PAIR"s};
			a_xs[-1] = f_cast<t_pair>(a_xs[0])->v_tail;
		});
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
		virtual void f_dump(const t_dump& a_dump) const
		{
			t_object::f_dump(a_dump);
		}
	};

	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments > 0) throw t_error{L"requires no arguments"s};
			a_xs[-1] = a_engine.f_new<t_instance>();
		});
	}
} v_gensym;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments > 0) throw t_error{L"requires no arguments"s};
			a_xs[-1] = a_engine.f_new<t_holder<t_module>>(a_engine, ""sv);
		});
	}
} v_module;

struct : t_static
{
	struct t_at_string : t_location
	{
		std::wstring v_source;
		t_at v_at;

		t_at_string(const std::wstring& a_source, const t_at& a_at) : v_source(a_source), v_at(a_at)
		{
		}
		virtual std::shared_ptr<t_location> f_at_head(t_pair* a_pair)
		{
			auto p = dynamic_cast<t_parsed_pair<std::wstring>*>(a_pair);
			return p ? std::make_shared<t_at_string>(p->v_source, p->v_where_head) : shared_from_this();
		}
		virtual std::shared_ptr<t_location> f_at_tail(t_pair* a_pair)
		{
			auto p = dynamic_cast<t_parsed_pair<std::wstring>*>(a_pair);
			return p ? std::make_shared<t_at_string>(p->v_source, p->v_where_tail) : shared_from_this();
		}
		virtual void f_dump(const t_dump& a_dump) const
		{
			a_dump << L"at "sv;
			decltype(v_source.begin()) i;
			v_at.f_dump(a_dump, [&](long a_position)
			{
				i = v_source.begin() + a_position;
			}, [&]
			{
				return *i++;
			});
		}
	};

	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments > 1) throw t_error{L"requires [EOF]"s};
			std::wcout << L"> ";
			std::wstring cs;
			std::getline(std::wcin, cs);
			if (cs.empty()) {
				a_xs[-1] = !std::wcin && a_arguments > 0 ? a_xs[0] : nullptr;
				return;
			}
			cs.push_back(WEOF);
			auto parse = [&](auto&& a_get, auto&& a_pair, auto&& a_location)
			{
				return t_parser<decltype(a_get), decltype(a_pair), decltype(a_location)>(a_engine, std::move(a_get), std::move(a_pair), std::move(a_location)).f_expression();
			};
			a_xs[-1] = parse([i = cs.begin()]() mutable
			{
				return *i++;
			}, [&](t_object* a_value, const t_at& a_at)
			{
				return a_engine.f_new<t_parsed_pair<std::wstring>>(a_engine.f_pointer(a_value), cs, a_at);
			}, [&](const t_at& a_at)
			{
				return std::make_shared<t_at_string>(cs, a_at);
			});
		});
	}
} v_read;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments != 2) throw t_error{L"requires OBJECT MODULE"s};
			if (!a_xs[0]) {
				a_xs[-1] = nullptr;
				return;
			}
			auto module = a_engine.f_pointer(f_cast<t_holder<t_module>>(a_xs[1]));
			auto code = a_engine.f_pointer(a_engine.f_new<t_holder<t_code>>(a_engine, nullptr, module));
			(*code)->v_imports.push_back(a_engine.v_global);
			(*code)->v_imports.push_back(module);
			{
				t_emit emit{*code};
				a_xs[0]->f_render(**code, std::make_shared<t_at_expression>(a_engine, a_xs[0]))->f_emit(emit, 0, true);
				emit.f_end();
			}
			a_engine.f_run(*code, nullptr);
			a_xs[-1] = a_engine.v_used[0];
		});
	}
} v_eval;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		a_engine.v_used -= a_arguments;
		if (a_arguments > 0)
			for (size_t i = 0;;) {
				std::wcout << a_engine.v_used[i];
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
		size_t v_frames;

		t_continuation(t_frame* a_frame, size_t a_stack, size_t a_frames) : v_stack(a_stack), v_frames(a_frames)
		{
			auto head = a_frame[v_frames - 1].v_stack;
			auto frames = reinterpret_cast<t_frame*>(std::copy_n(head, v_stack, reinterpret_cast<t_object**>(this + 1)));
			std::copy_n(a_frame, v_frames, frames);
			for (size_t i = 0; i < v_frames; ++i) frames[i].v_stack -= head - static_cast<t_object**>(nullptr);
		}
		virtual size_t f_size() const
		{
			return sizeof(t_continuation) + sizeof(t_object*) * v_stack + sizeof(t_frame) * v_frames;
		}
		virtual void f_scan(gc::t_collector& a_collector)
		{
			auto p = reinterpret_cast<t_object**>(this + 1);
			for (size_t i = 0; i < v_stack; ++i, ++p) *p = a_collector.f_forward(*p);
			auto q = reinterpret_cast<t_frame*>(p);
			for (size_t i = 0; i < v_frames; ++i, ++q) q->f_scan(a_collector);
		}
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			a_engine.v_used -= a_arguments + 1;
			if (a_arguments != 1) throw t_error{L"requires OBJECT"s};
			auto used = a_engine.v_used;
			if (used + v_stack > a_engine.v_stack.get() + t_engine::V_STACK || a_engine.v_frame - v_frames < a_engine.v_frames.get()) throw t_error{L"stack overflow"s};
			auto value = used[1];
			auto p = reinterpret_cast<t_object**>(this + 1);
			a_engine.v_used = std::copy_n(p, v_stack, used);
			a_engine.v_frame -= v_frames;
			std::copy_n(reinterpret_cast<t_frame*>(p + v_stack), v_frames, a_engine.v_frame);
			for (size_t i = 0; i < v_frames; ++i) a_engine.v_frame[i].v_stack += used - static_cast<t_object**>(nullptr);
			*a_engine.v_used++ = value;
		}
	};
	struct t_call : t_static
	{
		inline static void* v_return = reinterpret_cast<void*>(e_instruction__RETURN);

		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments != 3) {
				a_engine.v_used -= a_arguments + 1;
				throw t_error{L"requires TAG HANDLER THUNK"s};
			}
			if (a_engine.v_frame <= a_engine.v_frames.get()) {
				a_engine.v_used -= 4;
				throw t_error{L"stack overflow"s};
			}
			--a_engine.v_frame;
			a_engine.v_frame->v_stack = a_engine.v_used - 4;
			a_engine.v_frame->v_code = nullptr;
			a_engine.v_frame->v_current = &v_return;
			a_engine.v_frame->v_scope = nullptr;
			a_engine.v_used[-1]->f_call(a_engine, 0);
		}
	} v_call;
	struct : t_static
	{
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			auto tail = a_engine.v_used - a_arguments - 1;
			try {
				if (a_arguments < 1) throw t_error{L"requires TAG [OBJECT...]"s};
				auto frame = a_engine.v_frame;
				while (!dynamic_cast<t_call*>(frame->v_stack[0]) || frame->v_stack[1] != tail[1])
					if (++frame == a_engine.v_frames.get() + t_engine::V_FRAMES)
						throw t_error{L"no matching prompt found"s};
				auto head = frame->v_stack;
				auto stack = tail - head;
				auto frames = ++frame - a_engine.v_frame;
				head[1] = new(a_engine.f_allocate(sizeof(t_continuation) + sizeof(t_object*) * stack + sizeof(t_frame) * frames)) t_continuation(a_engine.v_frame, stack, frames);
				head[0] = this;
				auto handler = head[2];
				a_engine.v_used = std::copy(tail + 2, a_engine.v_used, head + 2);
				a_engine.v_frame = frame;
				handler->f_call(a_engine, a_arguments);
			} catch (...) {
				a_engine.v_used = tail;
				throw;
			}
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
		auto last = a_engine.f_pointer(list.v_value);
		while (pair->v_tail) {
			pair = f_cast<t_pair>(pair->v_tail);
			f_push(a_engine, last, pair->v_head);
		}
		last->v_tail = tail;
		return list;
	}

	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments != 2) throw t_error{L"requires PAIR OBJECT"s};
			a_xs[-1] = f_append(a_engine, a_xs[0], a_xs[1]);
		});
	}
} v_append;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments != 1) throw t_error{L"requires OBJECT"s};
			a_xs[-1] = a_engine.f_new<t_quote>(a_xs[0]);
		});
	}
} v_quote;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments != 1) throw t_error{L"requires MESSAGE"s};
			std::wstringstream out;
			out << a_xs[0];
			a_xs[-1] = a_engine.f_new<t_error::t_holder>(t_error{out.str()});
		});
	}
} v_error;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		f_try(a_engine, a_arguments, [&](auto a_xs)
		{
			if (a_arguments != 2) throw t_error{L"requires CONTINUATION ERROR"s};
			auto continuation = f_cast<prompt::t_continuation>(a_xs[0]);
			auto& backtrace = f_cast<t_error::t_holder>(a_xs[1])->v_value->v_backtrace;
			auto p = reinterpret_cast<t_frame*>(reinterpret_cast<char*>(continuation + 1) + sizeof(t_object*) * continuation->v_stack);
			for (auto q = p + continuation->v_frames; p != q; ++p)
				if (p->v_code) backtrace.push_back((*p->v_code)->f_location(p->v_current));
			a_xs[-1] = a_xs[1];
		});
	}
} v_catch;

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		v_catch.f_call(a_engine, a_arguments);
		throw std::move(*f_cast<t_error::t_holder>(a_engine.v_used[1])->v_value);
	}
} v_rethrow;

}

t_object* f_unquasiquote(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_object* a_value)
{
	auto& engine = a_code.v_engine;
	if (auto p = dynamic_cast<t_pair*>(a_value)) {
		auto pair = engine.f_pointer(p);
		auto tail = engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(f_unquasiquote(a_code, a_location, pair->v_tail)), nullptr));
		if (auto p = dynamic_cast<t_unquote_splicing*>(pair->v_head))
			return engine.f_new<t_call>(engine.f_pointer(engine.f_new<t_pair>(&v_append,
				engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(a_code.f_render(p->v_value, a_location)), tail))
			)), a_location);
		else
			return engine.f_new<t_call>(engine.f_pointer(engine.f_new<t_pair>(&v_cons,
				engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(f_unquasiquote(a_code, a_location, pair->v_head)), tail))
			)), a_location);
	}
	if (auto p = dynamic_cast<t_quote*>(a_value))
		return engine.f_new<t_call>(engine.f_pointer(engine.f_new<t_pair>(&v_quote,
			engine.f_pointer(engine.f_new<t_pair>(engine.f_pointer(f_unquasiquote(a_code, a_location, p->v_value)), nullptr))
		)), a_location);
	if (auto p = dynamic_cast<t_unquote*>(a_value)) return a_code.f_render(p->v_value, a_location);
	return engine.f_new<t_quote>(engine.f_pointer(a_value));
}

void f_rethrow(t_engine& a_engine, t_object* a_thunk)
{
	*a_engine.v_used++ = &prompt::v_call;
	*a_engine.v_used++ = &v_catch;
	*a_engine.v_used++ = &v_rethrow;
	*a_engine.v_used++ = a_thunk;
	prompt::v_call.f_call(a_engine, 3);
}

void f_throw(t_engine& a_engine, t_error&& a_error)
{
	*a_engine.v_used++ = &prompt::v_abort;
	*a_engine.v_used++ = &v_catch;
	*a_engine.v_used++ = a_engine.f_new<t_error::t_holder>(std::move(a_error));
	prompt::v_abort.f_call(a_engine, 2);
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
	a_module.f_register(L"module"sv, &v_module);
	a_module.f_register(L"read"sv, &v_read);
	a_module.f_register(L"eval"sv, &v_eval);
	a_module.f_register(L"print"sv, &v_print);
	a_module.f_register(L"call-with-prompt"sv, &prompt::v_call);
	a_module.f_register(L"abort-to-prompt"sv, &prompt::v_abort);
	a_module.f_register(L"error"sv, &v_error);
	a_module.f_register(L"catch"sv, &v_catch);
}

}
