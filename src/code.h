#ifndef LILIS__CODE_H
#define LILIS__CODE_H

#include "engine.h"
#include <list>
#include <vector>

namespace lilis
{

struct t_bindings : std::map<t_symbol*, t_object*>
{
	void f_scan(gc::t_collector& a_collector)
	{
		std::map<t_symbol*, t_object*> xs;
		for (auto& x : *this) xs.emplace(a_collector.f_forward(x.first), a_collector.f_forward(x.second));
		swap(xs);
	}
	t_object* f_find(t_symbol* a_symbol) const
	{
		auto i = find(a_symbol);
		return i == end() ? nullptr : i->second;
	}
};

struct t_mutable
{
	virtual t_object* f_render(t_code& a_code, t_object* a_expression) = 0;
};

struct t_module : t_bindings
{
	struct t_variable : t_with_value<t_object_of<t_variable>, t_object>, t_mutable
	{
		using t_base::t_base;
		virtual t_object* f_render(t_code& a_code, t_object* a_expression);
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail);
		virtual void f_call(t_engine& a_engine, size_t a_arguments);
	};

	t_engine& v_engine;
	t_holder<t_module>* v_this;
	std::filesystem::path v_path;
	std::map<std::wstring, t_holder<t_module>*, std::less<>>::iterator v_entry;

	t_module(t_engine& a_engine, t_holder<t_module>* a_this, const std::filesystem::path& a_path) : v_engine(a_engine), v_this(a_this), v_path(a_path)
	{
	}
	~t_module()
	{
		if (v_entry != decltype(v_entry){}) v_engine.v_modules.erase(v_entry);
	}
	void f_scan()
	{
		t_bindings::f_scan(v_engine);
		v_this = v_engine.f_forward(v_this);
		if (v_entry != decltype(v_entry){}) v_entry->second = v_this;
	}
	void f_register(std::wstring_view a_name, t_object* a_value)
	{
		auto value = v_engine.f_pointer(a_value);
		emplace(v_engine.f_symbol(a_name), value);
	}
};

struct t_scope : t_object_of<t_scope>
{
	t_scope* v_outer;
	size_t v_size;

	t_scope(t_scope* a_outer, size_t a_size, t_object** a_stack, size_t a_arguments) : v_outer(a_outer), v_size(a_size)
	{
		std::fill_n(std::copy_n(a_stack, a_arguments, f_locals()), v_size - a_arguments, nullptr);
	}
	virtual size_t f_size() const;
	virtual void f_scan(gc::t_collector& a_collector);
	t_object** f_locals()
	{
		return reinterpret_cast<t_object**>(this + 1);
	}
};

struct t_code
{
	struct t_local : t_with_value<t_object_of<t_local>, t_holder<t_code>>, t_mutable
	{
		size_t v_index;

		t_local(t_holder<t_code>* a_code, size_t a_index) : t_base(a_code), v_index(a_index)
		{
		}
		size_t f_outer(t_code* a_code) const;
		virtual t_object* f_render(t_code& a_code, t_object* a_expression);
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail);
	};

	t_engine& v_engine;
	t_holder<t_code>* v_this;
	t_holder<t_code>* v_outer;
	t_holder<t_module>* v_module;
	std::vector<t_holder<t_module>*> v_imports;
	std::vector<t_symbol*> v_locals;
	size_t v_arguments = 0;
	bool v_rest = false;
	bool v_macro = false;
	t_bindings v_bindings;
	std::vector<void*> v_instructions;
	std::vector<size_t> v_objects;
	size_t v_stack = 1;

	t_code(t_engine& a_engine, t_holder<t_code>* a_this, t_holder<t_code>* a_outer, t_holder<t_module>* a_module) : v_engine(a_engine), v_this(a_this), v_outer(a_outer), v_module(a_module)
	{
	}
	void f_scan();
	t_object* f_render(t_object* a_value, const std::shared_ptr<t_location>& a_location)
	{
		return a_value ? a_value->f_render(*this, a_location) : v_engine.f_new<t_quote>(nullptr);
	}
	t_object* f_resolve(t_symbol* a_symbol, const std::shared_ptr<t_location>& a_location) const;
	void f_compile_body(const std::shared_ptr<t_location>& a_location, t_pair* a_body);
	void f_compile(const std::shared_ptr<t_location>& a_location, t_pair* a_pair);
	t_holder<t_code>* f_new() const
	{
		return v_engine.f_new<t_holder<t_code>>(v_engine, v_this, v_module);
	}
	void f_call(bool a_rest, t_scope* a_outer, size_t a_arguments)
	{
		if (a_rest) {
			if (a_arguments < v_arguments) throw std::runtime_error("too few arguments");
		} else {
			if (a_arguments != v_arguments) throw std::runtime_error("wrong number of arguments");
		}
		auto scope = v_engine.f_pointer(a_outer);
		auto p = v_engine.f_allocate(sizeof(t_scope) + sizeof(t_object) * v_locals.size());
		auto used = v_engine.v_used - a_arguments;
		scope = new(p) t_scope(scope, v_locals.size(), used, v_arguments);
		if (a_rest) {
			auto tail = v_engine.f_pointer(scope->f_locals()[v_arguments]);
			for (auto p = used + v_arguments; v_engine.v_used != p; --v_engine.v_used)
				tail = scope->f_locals()[v_arguments] = v_engine.f_new<t_pair>(v_engine.v_used[-1], tail);
		}
		v_engine.v_used = used;
		if (v_engine.v_frame <= v_engine.v_frames.get()) throw std::runtime_error("stack overflow");
		--v_engine.v_frame;
		v_engine.v_frame->v_stack = --v_engine.v_used;
		if (v_engine.v_frame->v_stack + v_stack > v_engine.v_stack.get() + t_engine::V_STACK) throw std::runtime_error("stack overflow");
		v_engine.v_used = v_engine.v_frame->v_stack + 1;
		v_engine.v_frame->v_code = v_this;
		v_engine.v_frame->v_current = v_instructions.data();
		v_engine.v_frame->v_scope = scope;
	}
};

struct t_at
{
	long v_position;
	size_t v_line;
	size_t v_column;

	template<typename T_seek, typename T_get>
	void f_print(const char* a_path, T_seek a_seek, T_get a_get) const
	{
		a_seek(v_position);
		std::wcerr << L"at " << a_path << L':' << v_line << L':' << v_column << std::endl << L'\t';
		while (true) {
			wint_t c = a_get();
			if (c == WEOF || c == L'\n') break;
			std::wcerr << wchar_t(c);
		}
		a_seek(v_position);
		std::wcerr << std::endl << L'\t';
		for (size_t i = 1; i < v_column; ++i) {
			wint_t c = a_get();
			std::wcerr << (std::iswspace(c) ? wchar_t(c) : L' ');
		}
		std::wcerr << L'^' << std::endl;
	}
};

struct t_parsed_pair : t_object_of<t_parsed_pair, t_pair>
{
	std::filesystem::path v_path;
	t_at v_where_head;
	t_at v_where_tail;

	t_parsed_pair(t_object* a_head, const std::filesystem::path& a_path, const t_at& a_where_head) : t_object_of<t_parsed_pair, t_pair>(a_head, nullptr), v_path(a_path), v_where_head(a_where_head)
	{
	}
};

struct t_error : std::runtime_error
{
	std::list<std::shared_ptr<t_location>> v_backtrace;

	using std::runtime_error::runtime_error;
	void f_dump() const;
};

template<typename T, typename U>
inline T* f_cast(U* a_p)
{
	auto p = dynamic_cast<T*>(a_p);
	if (!p) throw t_error("must be "s + typeid(T).name());
	return p;
}

struct t_location : std::enable_shared_from_this<t_location>
{
	virtual ~t_location() = default;
	virtual std::shared_ptr<t_location> f_at_head(t_pair* a_pair) = 0;
	virtual std::shared_ptr<t_location> f_at_tail(t_pair* a_pair) = 0;
	virtual void f_print() const = 0;
	template<typename T>
	auto f_try(T&& a_do)
	{
		try {
			return a_do();
		} catch (t_error& e) {
			e.v_backtrace.push_back(shared_from_this());
			throw;
		}
	}
	template<typename T, typename U>
	T* f_cast(U* a_p)
	{
		return f_try([&]
		{
			return lilis::f_cast<T>(a_p);
		});
	}
	template<typename T>
	T* f_cast_head(t_pair* a_p)
	{
		return f_at_head(a_p)->f_cast<T>(a_p->v_head);
	}
	template<typename T>
	T* f_cast_tail(t_pair* a_p)
	{
		return f_at_tail(a_p)->f_cast<T>(a_p->v_tail);
	}
	void f_nil_tail(t_pair* a_p)
	{
		f_at_tail(a_p)->f_try([&]
		{
			if (a_p->v_tail) throw t_error("must be nil");
		});
	}
};

struct t_at_file : t_location
{
	std::filesystem::path v_path;
	t_at v_at;

	t_at_file(const std::filesystem::path& a_path, const t_at& a_at) : v_path(a_path), v_at(a_at)
	{
	}
	virtual std::shared_ptr<t_location> f_at_head(t_pair* a_pair);
	virtual std::shared_ptr<t_location> f_at_tail(t_pair* a_pair);
	virtual void f_print() const;
};

struct t_at_expression : t_location, gc::t_root
{
	t_engine& v_engine;
	t_object* v_expression;

	t_at_expression(t_engine& a_engine, t_object* a_expression) : gc::t_root(a_engine), v_engine(a_engine), v_expression(a_expression)
	{
	}
	virtual void f_scan(gc::t_collector& a_collector);
	virtual std::shared_ptr<t_location> f_at_head(t_pair* a_pair);
	virtual std::shared_ptr<t_location> f_at_tail(t_pair* a_pair);
	virtual void f_print() const;
};

inline std::wostream& operator<<(std::wostream& a_out, t_object* a_value)
{
	f_dump(a_value, {
		[&](auto x)
		{
			a_out << x;
		},
		[&](auto)
		{
		},
		[&](auto)
		{
		}
	});
	return a_out;
}

struct t_call : t_with_value<t_object_of<t_call>, t_pair>
{
	bool v_expand = false;

	using t_base::t_base;
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail);
};

enum t_instruction
{
	e_instruction__POP,
	e_instruction__PUSH,
	e_instruction__GET,
	e_instruction__SET,
	e_instruction__CALL,
	e_instruction__CALL_WITH_EXPANSION,
	e_instruction__CALL_TAIL,
	e_instruction__CALL_TAIL_WITH_EXPANSION,
	e_instruction__RETURN,
	e_instruction__LAMBDA,
	e_instruction__LAMBDA_WITH_REST,
	e_instruction__JUMP,
	e_instruction__BRANCH,
	e_instruction__END
};

struct t_emit
{
	struct t_label : std::vector<size_t>
	{
		size_t v_target;
	};

	t_code* v_code;
	t_holder<t_code>* v_boundary = nullptr;
	std::list<t_label> v_labels;

	t_emit& operator()(t_instruction a_instruction, size_t a_stack)
	{
		v_code->v_instructions.push_back(reinterpret_cast<void*>(a_instruction));
		if (a_stack > v_code->v_stack) v_code->v_stack = a_stack;
		return *this;
	}
	t_emit& operator()(size_t a_value)
	{
		v_code->v_instructions.push_back(reinterpret_cast<void*>(a_value));
		return *this;
	}
	t_emit& operator()(t_object* a_value)
	{
		v_code->v_objects.push_back(v_code->v_instructions.size());
		v_code->v_instructions.push_back(static_cast<void*>(a_value));
		return *this;
	}
	t_emit& operator()(t_label& a_value)
	{
		a_value.push_back(v_code->v_instructions.size());
		v_code->v_instructions.push_back(nullptr);
		return *this;
	}
	void f_end()
	{
		(*this)(e_instruction__RETURN, 0);
		for (auto& x : v_labels) {
			auto p = v_code->v_instructions.data() + x.v_target;
			for (auto i : x) v_code->v_instructions[i] = p;
		}
	}
};

}

#endif
