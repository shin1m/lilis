#include <algorithm>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cwctype>

namespace lilis
{

using namespace std::literals;

struct t_engine;
struct t_context;
struct t_symbol;
struct t_pair;
struct t_scope;
struct t_module;
struct t_code;
struct t_emit;

struct t_object
{
	virtual size_t f_size() const
	{
		throw std::runtime_error("invalid");
	}
	virtual t_object* f_forward(t_engine& a_engine) = 0;
	virtual void f_scan(t_engine& a_engine)
	{
	}
	virtual void f_destruct(t_engine& a_engine)
	{
	}
	virtual t_object* f_generate(t_code* a_code)
	{
		return this;
	}
	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments);
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail);
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		throw std::runtime_error("not callable");
	}
	virtual std::wstring f_string() const
	{
		return L"#object";
	}
};

struct t_root
{
	t_root* v_previous;
	t_root* v_next;

	t_root() : v_previous(this), v_next(this)
	{
	}
	t_root(t_root& a_root) : v_previous(&a_root), v_next(a_root.v_next)
	{
		v_previous->v_next = v_next->v_previous = this;
	}
	t_root(const t_root&) = delete;
	~t_root()
	{
		v_previous->v_next = v_next;
		v_next->v_previous = v_previous;
	}
	t_root& operator=(const t_root&) = delete;
	virtual void f_scan(t_engine& a_engine) = 0;
};

template<typename T>
struct t_pointer : t_root
{
	T* v_value;

	t_pointer(t_engine& a_engine, T* a_value) : t_root(a_engine), v_value(a_value)
	{
	}
	t_pointer& operator=(T* a_value)
	{
		v_value = a_value;
		return *this;
	}
	virtual void f_scan(t_engine& a_engine);
	operator T*() const
	{
		return v_value;
	}
	T* operator->() const
	{
		return v_value;
	}
};

template<typename T>
struct t_object_of : t_object
{
	virtual size_t f_size() const;
	virtual t_object* f_forward(t_engine& a_engine);
};

template<typename T>
struct t_holder : t_object_of<t_holder<T>>
{
	T* v_holdee;

	template<typename... T_an>
	t_holder(t_engine& a_engine, T_an&&... a_an) : v_holdee(new T(a_engine, this, std::forward<T_an>(a_an)...))
	{
	}
	virtual void f_scan(t_engine& a_engine)
	{
		v_holdee->f_scan();
	}
	virtual void f_destruct(t_engine& a_engine)
	{
		delete v_holdee;
	}
	operator T*() const
	{
		return v_holdee;
	}
	T* operator->() const
	{
		return v_holdee;
	}
};

struct t_engine : t_root
{
	struct t_forward : t_object
	{
		t_object* v_moved;
		size_t v_size;

		t_forward(t_object* a_moved, size_t a_size) : v_moved(a_moved), v_size(a_size)
		{
		}
		virtual size_t f_size() const
		{
			return v_size;
		}
		virtual t_object* f_forward(t_engine& a_engine)
		{
			return v_moved;
		}
	};

	static constexpr size_t V_STACK = 1024;
	static constexpr size_t V_CONTEXTS = 256;

	size_t v_size = 1024;
	std::unique_ptr<char[]> v_heap0{new char[v_size]};
	std::unique_ptr<char[]> v_heap1{new char[v_size]};
	char* v_head = v_heap0.get();
	char* v_tail = v_head + v_size;
	std::unique_ptr<t_object*[]> v_stack{new t_object*[V_STACK]};
	std::unique_ptr<t_context[]> v_contexts;
	t_object** v_used = v_stack.get();
	t_context* v_context;
	std::map<std::wstring, t_symbol*, std::less<>> v_symbols;
	t_holder<t_module>* v_global = nullptr;
	std::map<std::wstring, t_holder<t_module>*, std::less<>> v_modules;
	bool v_debug;
	bool v_verbose;

	t_engine(bool a_debug, bool a_verbose);
	t_object* f_move(t_object* a_p)
	{
		size_t n = a_p->f_size();
		auto p = v_tail;
		v_tail = std::copy_n(reinterpret_cast<char*>(a_p), n, p);
		new(a_p) t_forward(reinterpret_cast<t_object*>(p), n);
		return reinterpret_cast<t_object*>(p);
	}
	template<typename T>
	T* f_forward(T* a_value)
	{
		return a_value ? static_cast<T*>(a_value->f_forward(*this)) : nullptr;
	}
	void f_compact()
	{
		v_heap0.swap(v_heap1);
		v_head = v_tail = v_heap0.get();
		{
			t_root* p = this;
			do p->f_scan(*this); while ((p = p->v_next) != this);
		}
		while (v_head != v_tail) {
			auto p = reinterpret_cast<t_object*>(v_head);
			v_head += p->f_size();
			p->f_scan(*this);
		}
		v_tail = v_heap0.get() + v_size;
	}
	char* f_allocate(size_t a_n)
	{
		if (a_n < sizeof(t_forward)) a_n = sizeof(t_forward);
		auto p = v_head;
		v_head += a_n;
		if (v_head > v_tail || v_debug) {
			if (v_verbose) std::fprintf(stderr, "gc collecting...\n");
			f_compact();
			for (auto q = v_heap1.get(); q != p;) {
				auto p = reinterpret_cast<t_object*>(q);
				q += p->f_size();
				p->f_destruct(*this);
			}
			if (v_verbose) std::fprintf(stderr, "gc done: %d bytes free\n", v_tail - v_head);
			while (true) {
				p = v_head;
				v_head += a_n;
				if (v_head <= v_tail) break;
				if (v_verbose) std::fprintf(stderr, "gc expanding...\n");
				v_size *= 2;
				v_heap1.reset(new char[v_size]);
				f_compact();
				v_heap1.reset(new char[v_size]);
				if (v_verbose) std::fprintf(stderr, "gc done: %d bytes free\n", v_tail - v_head);
			}
		}
		return p;
	}
	template<typename T, typename... T_an>
	T* f_new(T_an&&... a_an)
	{
		auto p = f_allocate(sizeof(T));
		return new(p) T(std::forward<T_an>(a_an)...);
	}
	template<typename T>
	t_pointer<T> f_pointer(T* a_value)
	{
		return {*this, a_value};
	}
	virtual void f_scan(t_engine& a_engine);
	t_symbol* f_symbol(std::wstring_view a_name);
	void f_run(t_code* a_code, t_object* a_arguments);
	t_pair* f_parse(const char* a_path);
	void f_run(t_holder<t_module>* a_module, t_pair* a_expressions);
	t_holder<t_module>* f_module(const std::filesystem::path& a_path, std::wstring_view a_name);
};

template<typename T>
void t_pointer<T>::f_scan(t_engine& a_engine)
{
	v_value = a_engine.f_forward(v_value);
}

template<typename T>
size_t t_object_of<T>::f_size() const
{
	return std::max(sizeof(T), sizeof(t_engine::t_forward));
}

template<typename T>
t_object* t_object_of<T>::f_forward(t_engine& a_engine)
{
	return a_engine.f_move(this);
}

inline std::wstring f_string(t_object* a_value)
{
	return a_value ? a_value->f_string() : L"()"s;
}

struct t_symbol : t_object_of<t_symbol>
{
	std::map<std::wstring, t_symbol*, std::less<>>::iterator v_entry;

	t_symbol(std::map<std::wstring, t_symbol*, std::less<>>::iterator a_entry) : v_entry(a_entry)
	{
	}
	virtual void f_scan(t_engine& a_engine)
	{
		v_entry->second = this;
	}
	virtual void f_destruct(t_engine& a_engine)
	{
		a_engine.v_symbols.erase(v_entry);
	}
	virtual t_object* f_generate(t_code* a_code);
	virtual std::wstring f_string() const
	{
		return v_entry->first;
	}
};

struct t_pair : t_object_of<t_pair>
{
	t_object* v_head;
	t_object* v_tail;

	t_pair(t_object* a_head, t_object* a_tail) : v_head(a_head), v_tail(a_tail)
	{
	}
	virtual void f_scan(t_engine& a_engine)
	{
		v_head = a_engine.f_forward(v_head);
		v_tail = a_engine.f_forward(v_tail);
	}
	virtual t_object* f_generate(t_code* a_code);
	virtual std::wstring f_string() const
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
};

struct t_scope : t_object_of<t_scope>
{
	t_scope* v_outer;
	size_t v_size;

	t_scope(t_scope* a_outer, size_t a_size, t_object** a_stack, size_t a_arguments) : v_outer(a_outer), v_size(a_size)
	{
		std::fill_n(std::copy_n(a_stack, a_arguments, f_locals()), v_size - a_arguments, nullptr);
	}
	virtual size_t f_size() const
	{
		return sizeof(t_scope) + sizeof(t_object*) * v_size;
	}
	virtual void f_scan(t_engine& a_engine)
	{
		v_outer = a_engine.f_forward(v_outer);
		auto p = f_locals();
		for (size_t i = 0; i < v_size; ++i) p[i] = a_engine.f_forward(p[i]);
	}
	t_object** f_locals()
	{
		return reinterpret_cast<t_object**>(this + 1);
	}
};

struct t_bindings : std::map<t_symbol*, t_object*>
{
	void f_scan(t_engine& a_engine)
	{
		std::map<t_symbol*, t_object*> xs;
		for (auto& x : *this) xs.emplace(a_engine.f_forward(x.first), a_engine.f_forward(x.second));
		swap(xs);
	}
	t_object* f_find(t_symbol* a_symbol) const
	{
		auto i = find(a_symbol);
		return i == end() ? nullptr : i->second;
	}
};

template<typename T_base, typename T>
struct t_with_value : T_base
{
	using t_base = t_with_value<T_base, T>;

	T* v_value;

	t_with_value(T* a_value) : v_value(a_value)
	{
	}
	virtual void f_scan(t_engine& a_engine)
	{
		v_value = a_engine.f_forward(v_value);
	}
};

struct t_mutable
{
	virtual t_object* f_generate(t_code* a_code, t_object* a_expression) = 0;
};

struct t_module : t_bindings
{
	struct t_variable : t_with_value<t_object_of<t_variable>, t_object>, t_mutable
	{
		using t_base::t_base;
		virtual t_object* f_generate(t_code* a_code, t_object* a_expression);
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail);
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			switch (a_arguments) {
			case 0:
				break;
			case 1:
				v_value = *--a_engine.v_used;
				break;
			default:
				throw std::runtime_error("wrong number of arguments");
			}
			a_engine.v_used[-1] = v_value;
		}
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

enum t_instruction
{
	e_instruction__POP,
	e_instruction__PUSH,
	e_instruction__GET,
	e_instruction__SET,
	e_instruction__CALL,
	e_instruction__CALL_TAIL,
	e_instruction__RETURN,
	e_instruction__LAMBDA,
	e_instruction__LAMBDA_WITH_REST,
	e_instruction__JUMP,
	e_instruction__BRANCH,
	e_instruction__END
};

struct t_code
{
	struct t_local : t_with_value<t_object_of<t_local>, t_holder<t_code>>, t_mutable
	{
		struct t_set : t_with_value<t_object_of<t_set>, t_local>
		{
			t_object* v_expression;

			t_set(t_local* a_local, t_object* a_expression) : t_base(a_local), v_expression(a_expression)
			{
			}
			virtual void f_scan(t_engine& a_engine)
			{
				t_base::f_scan(a_engine);
				v_expression = a_engine.f_forward(v_expression);
			}
			virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail);
		};

		size_t v_index;

		t_local(t_holder<t_code>* a_code, size_t a_index) : t_base(a_code), v_index(a_index)
		{
		}
		size_t f_outer(t_code* a_code) const
		{
			for (size_t i = 0;; ++i) {
				if (a_code == *v_value) return i;
				if (!a_code->v_outer) throw std::runtime_error("out of scope");
				a_code = *a_code->v_outer;
			}
		}
		virtual t_object* f_generate(t_code* a_code, t_object* a_expression)
		{
			auto& engine = a_code->v_engine;
			return engine.f_new<t_set>(engine.f_pointer(this), engine.f_pointer(a_expression));
		}
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
	t_bindings v_bindings;
	std::vector<void*> v_instructions;
	std::vector<size_t> v_objects;
	size_t v_stack = 1;

	t_code(t_engine& a_engine, t_holder<t_code>* a_this, t_holder<t_code>* a_outer, t_holder<t_module>* a_module) : v_engine(a_engine), v_this(a_this), v_outer(a_outer), v_module(a_module)
	{
	}
	void f_scan();
	t_object* f_generate(t_object* a_value);
	t_object* f_resolve(t_symbol* a_symbol) const
	{
		for (auto code = this;; code = *code->v_outer) {
			if (auto p = code->v_bindings.f_find(a_symbol)) return p;
			for (auto i = code->v_imports.rbegin(); i != code->v_imports.rend(); ++i)
				if (auto p = (**i)->f_find(a_symbol)) return p;
			if (!code->v_outer) throw std::runtime_error("not found");
		}
	}
	void f_compile(t_object* a_source);
	void f_call(bool a_rest, t_scope* a_outer, size_t a_arguments);
};

struct t_context
{
	t_holder<t_code>* v_code;
	void** v_current;
	t_scope* v_scope;
	t_object** v_stack;

	void f_scan(t_engine& a_engine)
	{
		v_code = a_engine.f_forward(v_code);
		v_scope = a_engine.f_forward(v_scope);
	}
};

inline t_engine::t_engine(bool a_debug, bool a_verbose) : v_contexts(new t_context[V_CONTEXTS]), v_context(v_contexts.get() + V_CONTEXTS), v_debug(a_debug), v_verbose(a_verbose)
{
	v_global = f_new<t_holder<t_module>>(*this, std::filesystem::path{});
}

inline void t_code::f_call(bool a_rest, t_scope* a_outer, size_t a_arguments)
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
	if (v_engine.v_context <= v_engine.v_contexts.get()) throw std::runtime_error("stack overflow");
	--v_engine.v_context;
	v_engine.v_context->v_stack = --v_engine.v_used;
	if (v_engine.v_context->v_stack + v_stack > v_engine.v_stack.get() + t_engine::V_STACK) throw std::runtime_error("stack overflow");
	v_engine.v_used = v_engine.v_context->v_stack + 1;
	v_engine.v_context->v_code = v_this;
	v_engine.v_context->v_current = v_instructions.data();
	v_engine.v_context->v_scope = scope;
}

struct t_lambda : t_object_of<t_lambda>
{
	t_holder<t_code>* v_code;
	t_scope* v_scope;

	t_lambda(t_holder<t_code>* a_code, t_scope* a_scope) : v_code(a_code), v_scope(a_scope)
	{
	}
	virtual void f_scan(t_engine& a_engine)
	{
		v_code = a_engine.f_forward(v_code);
		v_scope = a_engine.f_forward(v_scope);
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

template<typename T, typename U>
inline T* f_cast(U* a_p)
{
	auto p = dynamic_cast<T*>(a_p);
	if (!p)
		throw std::runtime_error("must be "s + typeid(T).name());
	return p;
}

inline t_object* f_chop(t_pointer<t_object>& a_p)
{
	auto pair = f_cast<t_pair>(static_cast<t_object*>(a_p));
	auto p = pair->v_head;
	a_p = pair->v_tail;
	return p;
}

struct t_emit
{
	struct t_label : std::vector<size_t>
	{
		size_t v_target;
	};

	t_code* v_code;
	std::deque<t_label> v_labels;

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
};

void t_code::t_local::t_set::f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
{
	v_expression->f_emit(a_emit, a_stack, false);
	a_emit(e_instruction__SET, a_stack + 1)(v_value->f_outer(a_emit.v_code))(v_value->v_index);
}

void t_code::t_local::f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
{
	a_emit(e_instruction__GET, a_stack + 1)(f_outer(a_emit.v_code))(v_index);
}

struct t_call : t_object_of<t_call>
{
	t_object* v_callee;
	std::vector<t_object*> v_arguments;

	t_call(t_object* a_callee) : v_callee(a_callee)
	{
	}
	virtual void f_scan(t_engine& a_engine)
	{
		v_callee = a_engine.f_forward(v_callee);
		for (auto& x : v_arguments) x = a_engine.f_forward(x);
	}
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
	{
		v_callee->f_emit(a_emit, a_stack, false);
		auto n = a_stack;
		for (auto p : v_arguments) p->f_emit(a_emit, ++n, false);
		a_emit(a_tail ? e_instruction__CALL_TAIL : e_instruction__CALL, a_stack + 1)(v_arguments.size());
	}
};

struct t_quote : t_with_value<t_object_of<t_quote>, t_object>
{
	using t_base::t_base;
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
	{
		a_emit(e_instruction__PUSH, a_stack + 1)(v_value);
	}
	virtual std::wstring f_string() const
	{
		return L'\'' + lilis::f_string(v_value);
	}
};

inline t_object* t_code::f_generate(t_object* a_value)
{
	return a_value ? a_value->f_generate(this) : v_engine.f_new<t_quote>(nullptr);
}

void t_engine::f_scan(t_engine& a_engine)
{
	for (auto p = v_stack.get(); p != v_used; ++p) *p = f_forward(*p);
	for (auto p = v_context; p != v_contexts.get() + V_CONTEXTS; ++p) p->f_scan(*this);
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
	auto top = --v_context;
	top->v_code = nullptr;
	auto end = reinterpret_cast<void*>(e_instruction__END);
	top->v_current = &end;
	top->v_scope = nullptr;
	top->v_stack = v_used;
	*v_used++ = nullptr;
	{
		auto p = v_used;
		for (auto x = f_pointer(a_arguments); x;) *v_used++ = f_chop(x);
		a_code->f_call(false, nullptr, v_used - p);
	}
	while (true) {
		switch (static_cast<t_instruction>(reinterpret_cast<intptr_t>(*v_context->v_current))) {
		case e_instruction__POP:
			++v_context->v_current;
			--v_used;
			break;
		case e_instruction__PUSH:
			*v_used++ = static_cast<t_object*>(*++v_context->v_current);
			++v_context->v_current;
			break;
		case e_instruction__GET:
			{
				auto outer = reinterpret_cast<size_t>(*++v_context->v_current);
				auto index = reinterpret_cast<size_t>(*++v_context->v_current);
				++v_context->v_current;
				auto scope = v_context->v_scope;
				for (; outer > 0; --outer) scope = scope->v_outer;
				*v_used++ = scope->f_locals()[index];
			}
			break;
		case e_instruction__SET:
			{
				auto outer = reinterpret_cast<size_t>(*++v_context->v_current);
				auto index = reinterpret_cast<size_t>(*++v_context->v_current);
				++v_context->v_current;
				auto scope = v_context->v_scope;
				for (; outer > 0; --outer) scope = scope->v_outer;
				scope->f_locals()[index] = v_used[-1];
			}
			break;
		case e_instruction__CALL:
			{
				auto arguments = reinterpret_cast<size_t>(*++v_context->v_current);
				++v_context->v_current;
				auto callee = v_used[-1 - arguments];
				if (!callee) throw std::runtime_error("calling nil");
				callee->f_call(*this, arguments);
			}
			break;
		case e_instruction__CALL_TAIL:
			{
				auto arguments = reinterpret_cast<size_t>(*++v_context->v_current);
				v_used = std::copy(v_used - arguments - 1, v_used, v_context->v_stack);
				auto callee = *v_context++->v_stack;
				if (!callee) throw std::runtime_error("calling nil");
				callee->f_call(*this, arguments);
			}
			break;
		case e_instruction__RETURN:
			*v_context->v_stack = v_used[-1];
			v_used = v_context->v_stack + 1;
			++v_context;
			break;
		case e_instruction__LAMBDA:
			*v_used++ = f_new<t_lambda>(*reinterpret_cast<t_holder<t_code>**>(++v_context->v_current), v_context->v_scope);
			++v_context->v_current;
			break;
		case e_instruction__LAMBDA_WITH_REST:
			*v_used++ = f_new<t_lambda_with_rest>(*reinterpret_cast<t_holder<t_code>**>(++v_context->v_current), v_context->v_scope);
			++v_context->v_current;
			break;
		case e_instruction__JUMP:
			v_context->v_current = static_cast<void**>(*++v_context->v_current);
			break;
		case e_instruction__BRANCH:
			++v_context->v_current;
			if (*--v_used)
				++v_context->v_current;
			else
				v_context->v_current = static_cast<void**>(*v_context->v_current);
			break;
		case e_instruction__END:
			--v_used;
			return;
		}
	}
}

void t_engine::f_run(t_holder<t_module>* a_module, t_pair* a_expressions)
{
	auto module = f_pointer(a_module);
	auto expressions = f_pointer(a_expressions);
	auto code = f_pointer(f_new<t_holder<t_code>>(*this, nullptr, module));
	(*code)->v_imports.push_back(v_global);
	(*code)->f_compile(expressions);
	f_run(*code, nullptr);
}

t_object* t_object::f_apply(t_code* a_code, t_object* a_arguments)
{
	auto& engine = a_code->v_engine;
	auto arguments = engine.f_pointer(a_arguments);
	auto call = engine.f_pointer(engine.f_new<t_call>(engine.f_pointer(this)));
	while (arguments) {
		auto argument = a_code->f_generate(f_chop(arguments));
		call->v_arguments.push_back(argument);
	}
	return call;
}

void t_object::f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
{
	a_emit(e_instruction__PUSH, a_stack + 1)(this);
}

t_object* t_symbol::f_generate(t_code* a_code)
{
	return a_code->f_resolve(this);
}

t_object* t_pair::f_generate(t_code* a_code)
{
	auto tail = a_code->v_engine.f_pointer(v_tail);
	return a_code->f_generate(v_head)->f_apply(a_code, tail);
}

t_object* t_module::t_variable::f_generate(t_code* a_code, t_object* a_expression)
{
	auto& engine = a_code->v_engine;
	auto expression = engine.f_pointer(a_expression);
	auto p = engine.f_new<t_call>(engine.f_pointer(engine.f_new<t_quote>(engine.f_pointer(this))));
	p->v_arguments.push_back(expression);
	return p;
}

void t_module::t_variable::f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
{
	a_emit(e_instruction__PUSH, a_stack + 1)(this);
	a_emit(a_tail ? e_instruction__CALL_TAIL : e_instruction__CALL, a_stack + 1)(size_t(0));
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

void t_code::f_compile(t_object* a_source)
{
	auto body = v_engine.f_pointer(a_source);
	if (v_outer)
		for (auto arguments = v_engine.f_pointer(f_chop(body)); arguments;) {
			auto symbol = v_engine.f_pointer(dynamic_cast<t_symbol*>(static_cast<t_object*>(arguments)));
			if (symbol) {
				v_rest = true;
				arguments = nullptr;
			} else {
				symbol = f_cast<t_symbol>(f_chop(arguments));
				++v_arguments;
			}
			v_bindings.emplace(symbol, v_engine.f_new<t_local>(v_this, v_locals.size()));
			v_locals.push_back(symbol);
		}
	t_emit emit{this};
	if (body)
		while (true) {
			f_generate(f_chop(body))->f_emit(emit, 1, !body);
			if (!body) break;
			emit(e_instruction__POP, 0);
		}
	else
		emit(e_instruction__PUSH, 1)(static_cast<t_object*>(nullptr));
	emit(e_instruction__RETURN, 0);
	for (auto& x : emit.v_labels) {
		auto p = v_instructions.data() + x.v_target;
		for (auto i : x) v_instructions[i] = p;
	}
}

struct t_static : t_object
{
	virtual t_object* f_forward(t_engine& a_engine)
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

	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
	{
		auto& engine = a_code->v_engine;
		auto arguments = engine.f_pointer(a_arguments);
		auto code = engine.f_pointer(engine.f_new<t_holder<t_code>>(engine, a_code->v_this, nullptr));
		(*code)->f_compile(arguments);
		return engine.f_new<t_instantiate>(code);
	}
} v_lambda;

struct : t_static
{
	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
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
} v_define;

struct : t_static
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

	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
	{
		auto& engine = a_code->v_engine;
		auto arguments = engine.f_pointer(a_arguments);
		auto symbol = engine.f_pointer(f_cast<t_symbol>(f_chop(arguments)));
		auto code = engine.f_pointer(engine.f_new<t_holder<t_code>>(engine, a_code->v_this, nullptr));
		(*code)->f_compile(arguments);
		return a_code->v_bindings.emplace(symbol, engine.f_new<t_instance>(code)).first->second;
	}
} v_macro;

struct : t_static
{
	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
	{
		auto& engine = a_code->v_engine;
		auto arguments = engine.f_pointer(a_arguments);
		auto bound = engine.f_pointer(a_code->f_generate(f_chop(arguments)));
		auto value = a_code->f_generate(f_chop(arguments));
		if (arguments) throw std::runtime_error("must be nil");
		if (auto p = dynamic_cast<t_mutable*>(static_cast<t_object*>(bound))) return p->f_generate(a_code, value);
		throw std::runtime_error("not mutable");
	}
} v_set;

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
		virtual void f_scan(t_engine& a_engine)
		{
			v_condition = a_engine.f_forward(v_condition);
			v_true = a_engine.f_forward(v_true);
			v_false = a_engine.f_forward(v_false);
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

struct t_unquote : t_with_value<t_object_of<t_unquote>, t_object>
{
	using t_base::t_base;
	virtual std::wstring f_string() const
	{
		return L',' + lilis::f_string(v_value);
	}
};

struct t_unquote_splicing : t_unquote
{
	using t_unquote::t_unquote;
	virtual std::wstring f_string() const
	{
		return L",@" + lilis::f_string(v_value);
	}
};

struct t_quasiquote : t_with_value<t_object_of<t_quasiquote>, t_object>
{
	inline static struct : t_static
	{
		static t_object* f_append(t_engine& a_engine, t_object* a_list, t_object* a_tail)
		{
			if (!a_list) return a_tail;
			auto tail = a_engine.f_pointer(a_tail);
			auto pair = a_engine.f_pointer(f_cast<t_pair>(a_list));
			auto list = a_engine.f_pointer(a_engine.f_new<t_pair>(a_engine.f_pointer(pair->v_head), nullptr));
			auto last = a_engine.f_pointer<t_pair>(list);
			do {
				pair = f_cast<t_pair>(pair->v_tail);
				auto p = a_engine.f_new<t_pair>(a_engine.f_pointer(pair->v_head), nullptr);
				last->v_tail = p;
				last = p;
			} while (pair->v_tail);
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
	inline static struct : t_static
	{
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments != 1) throw std::runtime_error("requires an argument");
			a_engine.v_used[-2] = a_engine.f_new<t_quote>(a_engine.v_used[-1]);
			--a_engine.v_used;
		}
	} v_quote;

	static t_object* f_expand(t_code* a_code, t_object* a_value)
	{
		auto& engine = a_code->v_engine;
		if (auto p = dynamic_cast<t_pair*>(a_value)) {
			auto pair = engine.f_pointer(p);
			auto call = engine.f_pointer<t_call>(nullptr);
			if (auto p = dynamic_cast<t_unquote_splicing*>(pair->v_head)) {
				auto head = engine.f_pointer(a_code->f_generate(p->v_value));
				call = engine.f_new<t_call>(&v_append);
				call->v_arguments.push_back(head);
			} else {
				call = engine.f_new<t_call>(&v_cons);
				auto head = f_expand(a_code, pair->v_head);
				call->v_arguments.push_back(head);
			}
			auto tail = f_expand(a_code, pair->v_tail);
			call->v_arguments.push_back(tail);
			return call;
		}
		if (auto p = dynamic_cast<t_quote*>(a_value)) {
			auto value = engine.f_pointer(f_expand(a_code, p->v_value));
			auto call = engine.f_new<t_call>(&v_quote);
			call->v_arguments.push_back(value);
			return call;
		}
		if (auto p = dynamic_cast<t_unquote*>(a_value)) return a_code->f_generate(p->v_value);
		return engine.f_new<t_quote>(engine.f_pointer(a_value));
	}

	using t_base::t_base;
	virtual t_object* f_generate(t_code* a_code)
	{
		return f_expand(a_code, v_value);
	}
	virtual std::wstring f_string() const
	{
		return L'`' + lilis::f_string(v_value);
	}
};

struct : t_static
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		a_engine.v_used -= a_arguments;
		if (a_arguments > 0)
			for (size_t i = 0;;) {
				std::printf("%ls", lilis::f_string(a_engine.v_used[i]).c_str());
				if (++i >= a_arguments) break;
				std::putchar(' ');
			}
		std::putchar('\n');
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
		virtual void f_scan(t_engine& a_engine)
		{
			auto p = reinterpret_cast<t_object**>(this + 1);
			for (size_t i = 0; i < v_stack; ++i, ++p) *p = a_engine.f_forward(*p);
			auto q = reinterpret_cast<t_context*>(p);
			for (size_t i = 0; i < v_contexts; ++i, ++q) q->f_scan(a_engine);
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

template<typename T_seek, typename T_get>
void f_print_with_caret(T_seek a_seek, T_get a_get, std::FILE* a_out, size_t a_column)
{
	a_seek();
	std::putwc('\t', a_out);
	while (true) {
		wint_t c = a_get();
		if (c == WEOF || c == L'\n') break;
		std::putwc(c, a_out);
	}
	std::putwc('\n', a_out);
	a_seek();
	std::putwc('\t', a_out);
	for (size_t i = 1; i < a_column; ++i) {
		wint_t c = a_get();
		std::putwc(std::iswspace(c) ? c : ' ', a_out);
	}
	std::putwc('^', a_out);
	std::putwc('\n', a_out);
}

struct t_at
{
	long v_position;
	size_t v_line;
	size_t v_column;
};

struct t_error : std::runtime_error
{
	const t_at v_at;

	t_error(const char* a_message, const t_at& a_at) : std::runtime_error(a_message), v_at(a_at)
	{
	}
	template<typename T_seek, typename T_get>
	void f_dump(const char* a_path, T_seek a_seek, T_get a_get) const
	{
		std::fprintf(stderr, "at %s:%zu:%zu\n", a_path, v_at.v_line, v_at.v_column);
		f_print_with_caret([&]
		{
			a_seek(v_at.v_position);
		}, a_get, stderr, v_at.v_column);
	}
};

template<typename T_get>
struct t_parser
{
	t_engine& v_engine;
	T_get v_get;
	long v_p = 0;
	long v_position = 0;
	size_t v_line = 1;
	size_t v_column = 1;
	wint_t v_c;
	t_at v_at{0, 0, 0};

	void f_get()
	{
		++v_p;
		switch (v_c) {
		case L'\n':
			v_position = v_p;
			++v_line;
			v_column = 1;
			break;
		default:
			++v_column;
		}
		v_c = v_get();
	}
	void f_skip()
	{
		while (true) {
			while (std::iswspace(v_c)) f_get();
			if (v_c != L';') break;
			do {
				if (v_c == L'\n') {
					f_get();
					break;
				}
				f_get();
			} while (v_c != WEOF);
		}
		v_at = {v_position, v_line, v_column};
	}
	void f_next()
	{
		f_get();
		f_skip();
	}
	t_object* f_expression();
	t_pair* f_list()
	{
		f_next();
		auto head = v_engine.f_pointer<t_pair>(nullptr);
		if (v_c != L')') {
			head = v_engine.f_new<t_pair>(v_engine.f_pointer(f_expression()), nullptr);
			auto tail = v_engine.f_pointer<t_pair>(head);
			while (v_c != L')') {
				if (v_c == L'.') {
					f_next();
					tail->v_tail = f_expression();
					if (v_c != L')') throw t_error("must be ')'", v_at);
					break;
				}
				auto p = v_engine.f_new<t_pair>(v_engine.f_pointer(f_expression()), nullptr);
				tail->v_tail = p;
				tail = p;
			}
		}
		f_next();
		return head;
	}

	t_parser(t_engine& a_engine, T_get&& a_get) : v_engine(a_engine), v_get(std::forward<T_get>(a_get))
	{
		v_c = v_get();
		f_skip();
	}
	t_pair* operator()()
	{
		if (v_c == WEOF) return nullptr;
		auto head = v_engine.f_pointer(v_engine.f_new<t_pair>(v_engine.f_pointer(f_expression()), nullptr));
		auto tail = v_engine.f_pointer<t_pair>(head);
		while (v_c != WEOF) {
			auto p = v_engine.f_new<t_pair>(v_engine.f_pointer(f_expression()), nullptr);
			tail->v_tail = p;
			tail = p;
		}
		return head;
	}
};

template<typename T_get>
t_object* t_parser<T_get>::f_expression()
{
	switch (v_c) {
	case WEOF:
		throw t_error("unexpected end of file", v_at);
	case L'"':
		{
			f_get();
			std::vector<wchar_t> cs;
			while (v_c != WEOF) {
				if (v_c == L'"') {
					f_next();
					break;
				} else if (v_c == L'\\') {
					f_get();
					switch (v_c) {
					case L'"':
						cs.push_back(L'"');
						break;
					case L'0':
						cs.push_back(L'\0');
						break;
					case L'\\':
						cs.push_back(L'\\');
						break;
					case L'a':
						cs.push_back(L'\a');
						break;
					case L'b':
						cs.push_back(L'\b');
						break;
					case L'f':
						cs.push_back(L'\f');
						break;
					case L'n':
						cs.push_back(L'\n');
						break;
					case L'r':
						cs.push_back(L'\r');
						break;
					case L't':
						cs.push_back(L'\t');
						break;
					case L'v':
						cs.push_back(L'\v');
						break;
					default:
						throw t_error("lexical error", v_at);
					}
				} else {
					cs.push_back(v_c);
				}
				f_get();
			}
			return nullptr;
		}
	case L'\'':
		f_next();
		return v_engine.f_new<t_quote>(v_engine.f_pointer(f_expression()));
	case L'(':
		return f_list();
	case L',':
		f_get();
		if (v_c == L'@') {
			f_next();
			return v_engine.f_new<t_unquote_splicing>(v_engine.f_pointer(f_expression()));
		} else {
			f_skip();
			return v_engine.f_new<t_unquote>(v_engine.f_pointer(f_expression()));
		}
	case L'`':
		f_next();
		return v_engine.f_new<t_quasiquote>(v_engine.f_pointer(f_expression()));
	default:
		if (std::iswdigit(v_c)) {
			std::vector<wchar_t> cs;
			if (v_c == L'0') {
				cs.push_back(v_c);
				f_get();
				switch (v_c) {
				case L'.':
					break;
				case L'X':
				case L'x':
					cs.push_back(v_c);
					f_get();
					if (!std::iswxdigit(v_c)) throw t_error("lexical error", v_at);
					do {
						cs.push_back(v_c);
						f_get();
					} while (std::iswxdigit(v_c));
					cs.push_back(L'\0');
					//v_token = e_token__INTEGER;
					f_skip();
					return nullptr;
				default:
					while (std::iswdigit(v_c)) {
						if (v_c >= L'8') throw t_error("lexical error", v_at);
						cs.push_back(v_c);
						f_get();
					}
					cs.push_back(L'\0');
					//v_token = e_token__INTEGER;
					f_skip();
					return nullptr;
				}
			}
			while (std::iswdigit(v_c)) {
				cs.push_back(v_c);
				f_get();
			}
			if (v_c == L'.') {
				do {
					cs.push_back(v_c);
					f_get();
				} while (std::iswdigit(v_c));
				if (v_c == L'E' || v_c == L'e') {
					cs.push_back(v_c);
					f_get();
					if (v_c == L'+' || v_c == L'-') {
						cs.push_back(v_c);
						f_get();
					}
					if (!std::iswdigit(v_c)) throw t_error("lexical error", v_at);
					do {
						cs.push_back(v_c);
						f_get();
					} while (std::iswdigit(v_c));
				}
				cs.push_back(L'\0');
				//v_token = e_token__FLOAT;
				f_skip();
				return nullptr;
			} else {
				cs.push_back(L'\0');
				//v_token = e_token__INTEGER;
				f_skip();
				return nullptr;
			}
		} else {
			std::vector<wchar_t> cs;
			do {
				cs.push_back(v_c);
				f_get();
			} while (v_c != WEOF && !std::iswspace(v_c) && v_c != L')' && v_c != L';');
			f_skip();
			return v_engine.f_symbol({cs.data(), cs.size()});
		}
	}
}

template<typename T_get>
t_pair* f_parse(t_engine& a_engine, T_get&& a_get)
{
	return t_parser<T_get>(a_engine, std::forward<T_get>(a_get))();
}

t_pair* t_engine::f_parse(const char* a_path)
{
	auto fp = std::fopen(a_path, "r");
	if (fp == NULL) throw std::runtime_error("unable to open");
	std::unique_ptr<std::FILE, decltype(&std::fclose)> file(fp, &std::fclose);
	return lilis::f_parse(*this, [&]
	{
		return std::getwc(fp);
	});
}

t_holder<t_module>* t_engine::f_module(const std::filesystem::path& a_path, std::wstring_view a_name)
{
	auto i = v_modules.lower_bound(a_name);
	if (i != v_modules.end() && i->first == a_name) return i->second;
	auto path = a_path / a_name;
	path += ".lisp";
	auto expressions = f_pointer(f_parse(path.c_str()));
	i = v_modules.emplace_hint(i, a_name, nullptr);
	i->second = f_new<t_holder<t_module>>(*this, path);
	(*i->second)->v_entry = i;
	f_run(i->second, expressions);
	return i->second;
}

struct t_and_export : t_with_value<t_static, t_object>
{
	using t_base::t_base;
	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments)
	{
		auto& engine = a_code->v_engine;
		auto arguments = engine.f_pointer(a_arguments);
		auto value = engine.f_pointer(a_code->f_generate(v_value)->f_apply(a_code, arguments));
		if (a_code->v_outer) return value;
		auto symbol = engine.f_pointer(static_cast<t_symbol*>(static_cast<t_pair*>(static_cast<t_object*>(arguments))->v_head));
		if (dynamic_cast<t_mutable*>(a_code->f_resolve(symbol))) {
			auto variable = engine.f_new<t_module::t_variable>(nullptr);
			(*a_code->v_module)->insert_or_assign(symbol, variable);
			return variable->f_generate(a_code, value);
		} else {
			(*a_code->v_module)->insert_or_assign(symbol, value);
			return value;
		}
	}
};

}

int main(int argc, char* argv[])
{
	auto debug = false;
	auto verbose = false;
	{
		auto end = argv + argc;
		auto q = argv;
		for (auto p = argv; p < end; ++p) {
			if ((*p)[0] == '-' && (*p)[1] == '-') {
				const auto v = *p + 2;
				if (std::strcmp(v, "debug") == 0)
					debug = true;
				else if (std::strcmp(v, "verbose") == 0)
					verbose = true;
			} else {
				*q++ = *p;
			}
		}
		argc = q - argv;
	}
	using namespace lilis;
	t_engine engine(debug, verbose);
	{
		auto global = *engine.v_global;
		global->f_register(L"lambda"sv, &v_lambda);
		global->f_register(L"define-macro"sv, &v_macro);
		global->f_register(L"define"sv, &v_define);
		global->f_register(L"set!"sv, &v_set);
		global->f_register(L"export"sv, &v_export);
		global->f_register(L"import"sv, &v_import);
		global->f_register(L"if"sv, &v_if);
		global->f_register(L"eq?"sv, &v_eq);
		global->f_register(L"pair?"sv, &v_is_pair);
		global->f_register(L"cons"sv, &v_cons);
		global->f_register(L"car"sv, &v_car);
		global->f_register(L"cdr"sv, &v_cdr);
		global->f_register(L"print"sv, &v_print);
		global->f_register(L"call-with-prompt"sv, &prompt::v_call);
		global->f_register(L"abort-to-prompt"sv, &prompt::v_abort);
	}
	if (argc < 2) {
		auto module = engine.f_pointer(engine.f_new<t_holder<t_module>>(engine, ""sv));
		t_and_export v_macro_and_export(&v_macro);
		(*module)->f_register(L"define-macro"sv, &v_macro_and_export);
		t_and_export v_define_and_export(&v_define);
		(*module)->f_register(L"define"sv, &v_define_and_export);
		t_and_export v_set_and_export(&v_set);
		(*module)->f_register(L"set!"sv, &v_set_and_export);
		while (true) {
			std::fputs("> ", stdout);
			std::vector<wint_t> cs;
			auto c = std::getwchar();
			while (c != WEOF && c != L'\n') {
				cs.push_back(c);
				c = std::getwchar();
			}
			if (!cs.empty()) {
				cs.push_back(WEOF);
				try {
					auto expressions = engine.f_pointer(f_parse(engine, [i = cs.begin()]() mutable
					{
						return *i++;
					}));
					if (expressions) {
						auto code = engine.f_pointer(engine.f_new<t_holder<t_code>>(engine, nullptr, module));
						(*code)->v_imports.push_back(engine.v_global);
						(*code)->v_imports.push_back(module);
						(*code)->f_compile(expressions);
						engine.f_run(*code, nullptr);
						std::printf("%ls\n", f_string(engine.v_used[0]).c_str());
					}
				} catch (std::exception& e) {
					std::fprintf(stderr, "caught: %s\n", e.what());
					if (auto p = dynamic_cast<t_error*>(&e)) {
						decltype(cs.begin()) i;
						p->f_dump(nullptr, [&](long a_position)
						{
							i = cs.begin() + a_position;
						}, [&]
						{
							return *i++;
						});
					}
				}
			}
			if (c == WEOF) break;
		}
	} else {
		auto path = std::filesystem::absolute(argv[1]);
		auto module = engine.f_pointer(engine.f_new<t_holder<t_module>>(engine, path));
		try {
			engine.f_run(module, engine.f_parse(path.c_str()));
		} catch (std::exception& e) {
			std::fprintf(stderr, "caught: %s\n", e.what());
			if (auto p = dynamic_cast<t_error*>(&e)) {
				std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::fopen(path.c_str(), "r"), &std::fclose);
				p->f_dump(path.c_str(), [&](long a_position)
				{
					std::fseek(file.get(), a_position, SEEK_SET);
				}, [&]
				{
					return std::getwc(file.get());
				});
			}
			return -1;
		}
	}
	return 0;
}
