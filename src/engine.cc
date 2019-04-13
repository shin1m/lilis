#include "parser.h"
#include "builtins.h"
#include <fstream>

namespace lilis
{

void t_engine::f_scan(gc::t_collector& a_collector)
{
	for (auto p = v_stack.get(); p != v_used; ++p) *p = f_forward(*p);
	for (auto p = v_frame; p != v_frames.get() + V_FRAMES; ++p) p->f_scan(*this);
	v_global = f_forward(v_global);
}

t_symbol* t_engine::f_symbol(std::wstring_view a_name)
{
	auto i = v_symbols.lower_bound(a_name);
	if (i != v_symbols.end() && i->first == a_name) return i->second;
	i = v_symbols.emplace_hint(i, a_name, nullptr);
	return i->second = f_new<t_symbol>(i);
}

void t_engine::f_run(t_code* a_code, t_object* a_arguments)
{
	struct t_lambda : t_object_of<t_lambda>
	{
		t_holder<t_code>* v_code;
		t_scope* v_scope;

		t_lambda(t_holder<t_code>* a_code, t_scope* a_scope) : v_code(a_code), v_scope(a_scope)
		{
		}
		virtual void f_scan(gc::t_collector& a_collector)
		{
			v_code = a_collector.f_forward(v_code);
			v_scope = a_collector.f_forward(v_scope);
		}
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			(*v_code)->f_call(false, v_scope, a_arguments);
		}
	};
	struct t_lambda_with_rest : t_lambda
	{
		using t_lambda::t_lambda;
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			(*v_code)->f_call(true, v_scope, a_arguments);
		}
	};
	auto expand = [&](size_t a_arguments)
	{
		--a_arguments;
		if (auto last = *--v_used)
			while (true) {
				auto pair = f_cast<t_pair>(last);
				*v_used++ = pair->v_head;
				last = pair->v_tail;
				++a_arguments;
				if (!last) break;
				if (v_used >= v_stack.get() + V_STACK) throw t_error("stack overflow");
			}
		return a_arguments;
	};
	auto call = [&](bool a_expand)
	{
		auto arguments = reinterpret_cast<size_t>(*++v_frame->v_current);
		++v_frame->v_current;
		auto callee = v_used[-1 - arguments];
		if (!callee) throw t_error("calling nil");
		callee->f_call(*this, a_expand ? expand(arguments) : arguments);
	};
	auto tail = [&](bool a_expand)
	{
		auto arguments = reinterpret_cast<size_t>(*++v_frame->v_current);
		v_used = std::copy(v_used - arguments - 1, v_used, v_frame->v_stack);
		auto callee = *v_frame++->v_stack;
		if (!callee) throw t_error("calling nil");
		callee->f_call(*this, a_expand ? expand(arguments) : arguments);
	};
	auto used = v_used;
	auto frame = v_frame;
	try {
		auto end = reinterpret_cast<void*>(e_instruction__END);
		{
			auto top = --v_frame;
			top->v_code = nullptr;
			top->v_current = &end;
			top->v_scope = nullptr;
			top->v_stack = v_used;
		}
		*v_used++ = nullptr;
		{
			auto used = v_used;
			while (auto p = dynamic_cast<t_pair*>(a_arguments)) {
				*v_used++ = p->v_head;
				a_arguments = p->v_tail;
			}
			if (a_arguments) {
				*v_used++ = a_arguments;
				expand(v_used - used);
			}
			a_code->f_call(a_code->v_rest, nullptr, v_used - used);
		}
		while (true) {
			switch (static_cast<t_instruction>(reinterpret_cast<intptr_t>(*v_frame->v_current))) {
			case e_instruction__POP:
				++v_frame->v_current;
				--v_used;
				break;
			case e_instruction__PUSH:
				*v_used++ = static_cast<t_object*>(*++v_frame->v_current);
				++v_frame->v_current;
				break;
			case e_instruction__GET:
				{
					auto outer = reinterpret_cast<size_t>(*++v_frame->v_current);
					auto index = reinterpret_cast<size_t>(*++v_frame->v_current);
					++v_frame->v_current;
					auto scope = v_frame->v_scope;
					for (; outer > 0; --outer) scope = scope->v_outer;
					*v_used++ = scope->f_locals()[index];
				}
				break;
			case e_instruction__SET:
				{
					auto outer = reinterpret_cast<size_t>(*++v_frame->v_current);
					auto index = reinterpret_cast<size_t>(*++v_frame->v_current);
					++v_frame->v_current;
					auto scope = v_frame->v_scope;
					for (; outer > 0; --outer) scope = scope->v_outer;
					scope->f_locals()[index] = v_used[-1];
				}
				break;
			case e_instruction__CALL:
				call(false);
				break;
			case e_instruction__CALL_WITH_EXPANSION:
				call(true);
				break;
			case e_instruction__CALL_TAIL:
				tail(false);
				break;
			case e_instruction__CALL_TAIL_WITH_EXPANSION:
				tail(true);
				break;
			case e_instruction__RETURN:
				*v_frame->v_stack = v_used[-1];
				v_used = v_frame->v_stack + 1;
				++v_frame;
				break;
			case e_instruction__LAMBDA:
				*v_used++ = f_new<t_lambda>(*reinterpret_cast<t_holder<t_code>**>(++v_frame->v_current), v_frame->v_scope);
				++v_frame->v_current;
				break;
			case e_instruction__LAMBDA_WITH_REST:
				*v_used++ = f_new<t_lambda_with_rest>(*reinterpret_cast<t_holder<t_code>**>(++v_frame->v_current), v_frame->v_scope);
				++v_frame->v_current;
				break;
			case e_instruction__JUMP:
				v_frame->v_current = static_cast<void**>(*++v_frame->v_current);
				break;
			case e_instruction__BRANCH:
				++v_frame->v_current;
				if (*--v_used)
					++v_frame->v_current;
				else
					v_frame->v_current = static_cast<void**>(*v_frame->v_current);
				break;
			case e_instruction__END:
				--v_used;
				++v_frame;
				return;
			}
		}
	} catch (std::exception& e) {
		v_used = used;
		auto rethrow = [&](t_error& a_e)
		{
			for (--frame; v_frame < frame; ++v_frame) a_e.v_backtrace.push_back((*v_frame->v_code)->f_location(v_frame->v_current));
			++v_frame;
			throw a_e;
		};
		if (auto p = dynamic_cast<t_error*>(&e)) rethrow(*p);
		t_error error(e.what());
		rethrow(error);
	}
}

t_pair* t_engine::f_parse(const std::filesystem::path& a_path)
{
	std::wfilebuf fb;
	if (!fb.open(a_path, std::ios_base::in)) throw t_error("unable to open");
	return lilis::f_parse(*this, a_path, [&]
	{
		return fb.sbumpc();
	});
}

void t_engine::f_run(t_holder<t_module>* a_module, const gc::t_pointer<t_pair>& a_expressions)
{
	auto code = f_pointer(f_new<t_holder<t_code>>(*this, nullptr, f_pointer(a_module)));
	(*code)->v_imports.push_back(v_global);
	(*code)->f_compile_body(std::make_shared<t_at_file>(std::filesystem::path(), t_at()), a_expressions);
	f_run(*code, nullptr);
}

t_holder<t_module>* t_engine::f_module(const std::filesystem::path& a_path, std::wstring_view a_name)
{
	auto i = v_modules.lower_bound(a_name);
	if (i != v_modules.end() && i->first == a_name) return i->second;
	auto path = a_path / a_name;
	path += ".lisp";
	auto expressions = f_pointer(f_parse(path));
	i = v_modules.emplace_hint(i, a_name, nullptr);
	i->second = f_new<t_holder<t_module>>(*this, path);
	(*i->second)->v_entry = i;
	if (expressions) f_run(i->second, expressions);
	return i->second;
}

}
