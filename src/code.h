#ifndef LILIS__CODE_H
#define LILIS__CODE_H

#include "engine.h"
#include <list>
#include <vector>

namespace lilis
{

struct t_error
{
	struct t_holder : t_object_of<t_holder>
	{
		t_error* v_value;

		t_holder(t_error&& a_value) : v_value(new t_error(std::move(a_value)))
		{
		}
		virtual void f_destruct(gc::t_collector& a_collector);
		virtual void f_dump(const t_dump& a_dump) const;
	};

	std::wstring v_message;
	std::vector<std::shared_ptr<t_location>> v_backtrace;

	void f_dump(const t_dump& a_dump) const;
};

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
	struct t_address_location
	{
		size_t v_address;
		std::shared_ptr<t_location> v_location;

		bool operator<(size_t a_address) const
		{
			return v_address < a_address;
		}
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
	std::vector<t_address_location> v_locations;
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
	std::shared_ptr<t_location> f_location(void** a_address) const;
	void f_call(bool a_rest, t_scope* a_outer, size_t a_arguments)
	{
		auto used = v_engine.v_used - a_arguments;
		try {
			if (a_rest) {
				if (a_arguments < v_arguments) throw t_error{L"too few arguments"s};
			} else {
				if (a_arguments != v_arguments) throw t_error{L"wrong number of arguments"s};
			}
			auto scope = v_engine.f_pointer(a_outer);
			auto p = v_engine.f_allocate(sizeof(t_scope) + sizeof(t_object) * v_locals.size());
			scope = new(p) t_scope(scope, v_locals.size(), used, v_arguments);
			if (a_rest) {
				auto tail = v_engine.f_pointer(scope->f_locals()[v_arguments]);
				for (auto p = used + v_arguments; v_engine.v_used != p; --v_engine.v_used)
					tail = scope->f_locals()[v_arguments] = v_engine.f_new<t_pair>(v_engine.v_used[-1], tail);
			}
			v_engine.v_used = used--;
			if (used + v_stack > v_engine.v_stack.get() + t_engine::c_STACK || v_engine.v_frame <= v_engine.v_frames.get()) throw t_error{L"stack overflow"s};
			--v_engine.v_frame;
			v_engine.v_frame->v_stack = used;
			v_engine.v_frame->v_code = v_this;
			v_engine.v_frame->v_current = v_instructions.data();
			v_engine.v_frame->v_scope = scope;
		} catch (...) {
			v_engine.v_used = used - 1;
			throw;
		}
	}
};

struct t_at
{
	long v_position;
	size_t v_line;
	size_t v_column;

	template<typename T_seek, typename T_get>
	void f_dump(const t_dump& a_dump, T_seek a_seek, T_get a_get) const
	{
		a_dump << std::to_wstring(v_line) << L":"sv << std::to_wstring(v_column) << L"\n\t"sv;
		a_seek(v_position);
		while (true) {
			wint_t c = a_get();
			if (c == WEOF || c == L'\n') break;
			a_dump << wchar_t(c);
		}
		a_seek(v_position);
		a_dump << L"\n\t"sv;
		for (size_t i = 1; i < v_column; ++i) {
			wint_t c = a_get();
			a_dump << (std::iswspace(c) ? wchar_t(c) : L' ');
		}
		a_dump << L"^\n"sv;
	}
};

template<typename T, typename U>
inline T* f_cast(U* a_p)
{
	auto p = dynamic_cast<T*>(a_p);
	if (!p) throw t_error{L"must be "s + std::filesystem::path(typeid(T).name()).wstring()};
	return p;
}

struct t_location : std::enable_shared_from_this<t_location>
{
	virtual ~t_location() = default;
	virtual std::shared_ptr<t_location> f_at_head(t_pair* a_pair) = 0;
	virtual std::shared_ptr<t_location> f_at_tail(t_pair* a_pair) = 0;
	virtual void f_dump(const t_dump& a_dump) const = 0;
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
			if (a_p->v_tail) throw t_error{L"must be nil"s};
		});
	}
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
	virtual void f_dump(const t_dump& a_dump) const;
};

inline std::wostream& operator<<(std::wostream& a_out, t_object* a_value)
{
	t_dump{[&](auto x)
	{
		a_out << x;
	}, [&](auto)
	{
	}, [&](auto)
	{
	}} << a_value;
	return a_out;
}

struct t_call : t_with_value<t_object_of<t_call>, t_pair>
{
	std::shared_ptr<t_location> v_location;
	bool v_expand = false;

	t_call(t_pair* a_value, const std::shared_ptr<t_location>& a_location) : t_base(a_value), v_location(a_location)
	{
	}
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
	void f_at(const std::shared_ptr<t_location>& a_location)
	{
		v_code->v_locations.emplace_back(v_code->v_instructions.size(), a_location);
	}
};

}

#endif
