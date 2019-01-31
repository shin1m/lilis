#include <algorithm>
#include <deque>
#include <functional>
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
struct t_scope;
struct t_code;
namespace ast
{
struct t_node;
}

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
	virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code);
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		throw std::runtime_error("not callable");
	}
	virtual void f_tail(t_engine& a_engine, size_t a_arguments);
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

	static constexpr size_t V_HEAP = 1024 * 16;
	static constexpr size_t V_STACK = 1024;
	static constexpr size_t V_CONTEXTS = 256;

	std::unique_ptr<char[]> v_heap0{new char[V_HEAP]};
	std::unique_ptr<char[]> v_heap1{new char[V_HEAP]};
	char* v_head = v_heap0.get();
	char* v_tail = v_head + V_HEAP;
	std::unique_ptr<t_object*[]> v_stack{new t_object*[V_STACK]};
	std::unique_ptr<t_context[]> v_contexts;
	t_object** v_used = v_stack.get();
	t_context* v_context;
	std::map<std::wstring, t_symbol*, std::less<>> v_symbols;
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
	char* f_allocate(size_t a_n)
	{
		if (a_n < sizeof(t_forward)) a_n = sizeof(t_forward);
		auto p = v_head;
		v_head += a_n;
		if (v_head > v_tail || v_debug) {
			if (v_verbose) std::fprintf(stderr, "gc collecting...\n");
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
			for (auto q = v_heap1.get(); q != p;) {
				auto p = reinterpret_cast<t_object*>(q);
				q += p->f_size();
				p->f_destruct(*this);
			}
			v_tail = v_heap0.get() + V_HEAP;
			if (v_verbose) std::fprintf(stderr, "gc done: %d bytes free\n", v_tail - v_head);
			p = v_head;
			v_head += a_n;
			if (v_head > v_tail) throw std::runtime_error("out of memory");
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
	t_scope* f_run(t_code* a_code);
};

template<typename T>
void t_pointer<T>::f_scan(t_engine& a_engine)
{
	v_value = a_engine.f_forward(v_value);
}

template<typename T>
struct t_object_of : t_object
{
	virtual size_t f_size() const
	{
		return std::max(sizeof(T), sizeof(t_engine::t_forward));
	}
	virtual t_object* f_forward(t_engine& a_engine)
	{
		return a_engine.f_move(this);
	}
};

inline std::wstring f_string(t_object* a_value)
{
	return a_value ? a_value->f_string() : L"()";
}

struct t_symbol : t_object_of<t_symbol>
{
	std::map<std::wstring, t_symbol*, std::less<>>::iterator v_entry;

	t_symbol(std::map<std::wstring, t_symbol*, std::less<>>::iterator a_entry) : v_entry(a_entry)
	{
	}
	virtual void f_destruct(t_engine& a_engine)
	{
		a_engine.v_symbols.erase(v_entry);
	}
	virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code);
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
	virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code);
	virtual std::wstring f_string() const
	{
		std::wstring s = L"(";
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

struct t_binding
{
	virtual ~t_binding() = default;
	virtual void f_scan(t_engine& a_engine)
	{
	}
	virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code, size_t a_outer)
	{
		throw std::runtime_error("cannot generate");
	}
	virtual std::unique_ptr<t_binding> f_export(t_scope* a_scope)
	{
		throw std::runtime_error("cannot export");
	}
};

struct t_bindings : std::map<t_symbol*, std::unique_ptr<t_binding>>
{
	void f_scan(t_engine& a_engine)
	{
		std::map<t_symbol*, std::unique_ptr<t_binding>> xs;
		for (auto& x : *this) {
			x.second->f_scan(a_engine);
			xs.emplace(a_engine.f_forward(x.first), std::move(x.second));
		}
		*static_cast<std::map<t_symbol*, std::unique_ptr<t_binding>>*>(this) = std::move(xs);
	}
	t_binding* f_find(t_symbol* a_symbol) const
	{
		auto i = find(a_symbol);
		return i == end() ? nullptr : i->second.get();
	}
};

struct t_setter
{
	virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code, size_t a_outer, std::unique_ptr<ast::t_node>&& a_expression) = 0;
};

struct t_module : t_bindings
{
	struct t_variable : t_binding, t_setter
	{
		t_object* v_value;

		t_variable(t_object* a_value) : v_value(a_value)
		{
		}
		virtual void f_scan(t_engine& a_engine)
		{
			v_value = a_engine.f_forward(v_value);
		}
		virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code, size_t a_outer);
		virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code, size_t a_outer, std::unique_ptr<ast::t_node>&& a_expression);
	};
	struct t_get : t_object_of<t_get>
	{
		t_variable* v_variable;

		t_get(t_variable* a_variable) : v_variable(a_variable)
		{
		}
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments != 0) throw std::runtime_error("requires no argument");
			a_engine.v_used[-1] = v_variable->v_value;
		}
	};
	struct t_set : t_object_of<t_set>
	{
		t_variable* v_variable;

		t_set(t_variable* a_variable) : v_variable(a_variable)
		{
		}
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments != 1) throw std::runtime_error("requires an argument");
			--a_engine.v_used;
			a_engine.v_used[-1] = v_variable->v_value = a_engine.v_used[0];
		}
	};

	t_engine& v_engine;

	t_module(t_engine& a_engine, t_holder<t_module>* a_this) : v_engine(a_engine)
	{
	}
	void f_scan()
	{
		t_bindings::f_scan(v_engine);
	}
	void f_register(std::wstring_view a_name, t_binding* a_binding)
	{
		emplace(v_engine.f_symbol(a_name), a_binding);
	}
};

enum t_instruction
{
	e_instruction__POP,
	e_instruction__INSTANCE,
	e_instruction__GET,
	e_instruction__SET,
	e_instruction__CALL,
	e_instruction__CALL_TAIL,
	e_instruction__RETURN,
	e_instruction__LAMBDA,
	e_instruction__JUMP,
	e_instruction__BRANCH,
	e_instruction__END
};

struct t_binder
{
	virtual std::unique_ptr<ast::t_node> f_bind(t_code* a_code, t_object* a_arguments) = 0;
};

struct t_code
{
	struct t_lambda : t_binding
	{
		virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code, size_t a_outer);
	};
	struct t_local : t_binding, t_setter
	{
		size_t v_index;

		t_local(size_t a_index) : v_index(a_index)
		{
		}
		virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code, size_t a_outer);
		virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code, size_t a_outer, std::unique_ptr<ast::t_node>&& a_expression);
		virtual std::unique_ptr<t_binding> f_export(t_scope* a_scope)
		{
			return std::make_unique<t_module::t_variable>(a_scope->f_locals()[v_index]);
		}
	};

	t_engine& v_engine;
	t_holder<t_code>* v_this;
	t_holder<t_code>* v_outer;
	t_holder<t_module>* v_module;
	std::vector<t_holder<t_module>*> v_imports;
	std::vector<t_symbol*> v_locals;
	size_t v_arguments;
	t_bindings v_bindings;
	std::vector<void*> v_instructions;
	std::vector<size_t> v_objects;
	size_t v_stack = 1;

	t_code(t_engine& a_engine, t_holder<t_code>* a_this, t_holder<t_code>* a_outer, t_holder<t_module>* a_module) : v_engine(a_engine), v_this(a_this), v_outer(a_outer), v_module(a_module)
	{
	}
	void f_scan();
	std::pair<size_t, t_binding*> f_resolve(t_symbol* a_symbol) const
	{
		auto code = this;
		for (size_t outer = 0;; ++outer) {
			if (auto p = code->v_bindings.f_find(a_symbol)) return {outer, p};
			for (auto i = code->v_imports.rbegin(); i != code->v_imports.rend(); ++i)
				if (auto p = (**i)->f_find(a_symbol)) return {outer, p};
			if (!code->v_outer) break;
			code = *code->v_outer;
		}
		throw std::runtime_error("not found");
	}
	void f_compile(t_object* a_source);
	void f_call(t_scope* a_outer, size_t a_arguments, bool a_tail);
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
}

inline void t_code::f_call(t_scope* a_outer, size_t a_arguments, bool a_tail)
{
	if (a_arguments != v_arguments) throw std::runtime_error("wrong number of arguments");
	auto scope = v_engine.f_pointer(a_outer);
	auto p = v_engine.f_allocate(sizeof(t_scope) + sizeof(t_object) * v_locals.size());
	v_engine.v_used -= v_arguments;
	scope = new(p) t_scope(scope, v_locals.size(), v_engine.v_used, v_arguments);
	if (!a_tail) {
		if (v_engine.v_context <= v_engine.v_contexts.get()) throw std::runtime_error("stack overflow");
		--v_engine.v_context;
		v_engine.v_context->v_stack = --v_engine.v_used;
	}
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
		(*v_code)->f_call(v_scope, a_arguments, false);
	}
	virtual void f_tail(t_engine& a_engine, size_t a_arguments)
	{
		(*v_code)->f_call(v_scope, a_arguments, true);
	}
};

template<typename T, typename U>
inline T* require_cast(U* a_p, const char* a_message)
{
	auto p = dynamic_cast<T*>(a_p);
	if (!p) throw std::runtime_error(a_message);
	return p;
}

namespace ast
{

struct t_emit;

struct t_node
{
	virtual ~t_node() = default;
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
	{
		throw std::runtime_error("cannot emit");
	}
	virtual std::unique_ptr<t_node> f_apply(t_code* a_code, t_object* a_arguments);
};

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

struct t_lambda : t_node
{
	struct t_node : ast::t_node
	{
		t_pointer<t_holder<t_code>> v_code;

		t_node(t_holder<t_code>* a_code) : v_code((*a_code)->v_engine, a_code)
		{
		}
		virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
		{
			a_emit(e_instruction__LAMBDA, a_stack + 1)(v_code);
		}
	};

	virtual std::unique_ptr<ast::t_node> f_apply(t_code* a_code, t_object* a_arguments)
	{
		delete this;
		auto& engine = a_code->v_engine;
		auto arguments = engine.f_pointer(a_arguments);
		auto code = engine.f_pointer(engine.f_new<t_holder<t_code>>(engine, a_code->v_this, nullptr));
		(*code)->f_compile(arguments);
		return std::make_unique<t_node>(code);
	}
};

struct t_get : t_node
{
	size_t v_outer;
	size_t v_index;

	t_get(size_t a_outer, size_t a_index) : v_outer(a_outer), v_index(a_index)
	{
	}
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
	{
		a_emit(e_instruction__GET, a_stack + 1)(v_outer)(v_index);
	}
};

struct t_set : t_node
{
	size_t v_outer;
	size_t v_index;
	std::unique_ptr<t_node> v_expression;

	t_set(size_t a_outer, size_t a_index, std::unique_ptr<t_node>&& a_expression) : v_outer(a_outer), v_index(a_index), v_expression(std::move(a_expression))
	{
	}
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
	{
		v_expression->f_emit(a_emit, a_stack, false);
		a_emit(e_instruction__SET, a_stack + 1)(v_outer)(v_index);
	}
};

struct t_call : t_node
{
	std::unique_ptr<t_node> v_callee;
	std::vector<std::unique_ptr<t_node>> v_arguments;

	t_call(std::unique_ptr<t_node>&& a_callee) : v_callee(std::move(a_callee))
	{
	}
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
	{
		v_callee->f_emit(a_emit, a_stack, false);
		auto n = a_stack;
		for (auto& p : v_arguments) p->f_emit(a_emit, ++n, false);
		a_emit(a_tail ? e_instruction__CALL_TAIL : e_instruction__CALL, a_stack + 1)(v_arguments.size());
	}
};

struct t_quote : t_node
{
	t_pointer<t_object> v_value;

	t_quote(t_engine& a_engine, t_object* a_value) : v_value(a_engine, a_value)
	{
	}
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail)
	{
		a_emit(e_instruction__INSTANCE, a_stack + 1)(v_value);
	}
};

inline std::unique_ptr<t_node> f_generate(t_object* a_value, t_code* a_code)
{
	return a_value ? a_value->f_generate(a_code) : std::make_unique<t_quote>(a_code->v_engine, nullptr);
}

struct t_if : t_node
{
	std::unique_ptr<t_node> v_condition;
	std::unique_ptr<t_node> v_true;
	std::unique_ptr<t_node> v_false;

	t_if(std::unique_ptr<t_node>&& a_condition, std::unique_ptr<t_node>&& a_true, std::unique_ptr<t_node>&& a_false) : v_condition(std::move(a_condition)), v_true(std::move(a_true)), v_false(std::move(a_false))
	{
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
			a_emit(e_instruction__INSTANCE, a_stack + 1)(static_cast<t_object*>(nullptr));
		label1.v_target = a_emit.v_code->v_instructions.size();
	}
};

}

void t_engine::f_scan(t_engine& a_engine)
{
	for (auto p = v_stack.get(); p != v_used; ++p) *p = a_engine.f_forward(*p);
	for (auto p = v_context; p != v_contexts.get() + V_CONTEXTS; ++p) p->f_scan(*this);
	for (auto& x : v_symbols) x.second = f_forward(x.second);
}

t_symbol* t_engine::f_symbol(std::wstring_view a_name)
{
	auto i = v_symbols.lower_bound(a_name);
	if (i == v_symbols.end() || i->first != a_name) {
		i = v_symbols.emplace_hint(i, a_name, nullptr);
		i->second = f_new<t_symbol>(i);
	}
	return i->second;
}

t_scope* t_engine::f_run(t_code* a_code)
{
	auto top = --v_context;
	top->v_code = nullptr;
	auto end = reinterpret_cast<void*>(e_instruction__END);
	top->v_current = &end;
	top->v_scope = nullptr;
	top->v_stack = v_used = v_stack.get();
	*v_used++ = nullptr;
	a_code->f_call(nullptr, 0, false);
	auto scope = f_pointer(v_context->v_scope);
	while (true) {
		switch (static_cast<t_instruction>(reinterpret_cast<intptr_t>(*v_context->v_current))) {
		case e_instruction__POP:
			++v_context->v_current;
			--v_used;
			break;
		case e_instruction__INSTANCE:
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
				if (!callee) throw std::runtime_error("applying to nil");
				callee->f_call(*this, arguments);
			}
			break;
		case e_instruction__CALL_TAIL:
			{
				auto arguments = reinterpret_cast<size_t>(*++v_context->v_current);
				auto callee = v_used[-1 - arguments];
				if (!callee) throw std::runtime_error("applying to nil");
				callee->f_tail(*this, arguments);
			}
			break;
		case e_instruction__RETURN:
			*v_context->v_stack = v_used[-1];
			v_used = v_context->v_stack + 1;
			++v_context;
			break;
		case e_instruction__LAMBDA:
			*v_used++ = f_new<lilis::t_lambda>(*reinterpret_cast<t_holder<t_code>**>(++v_context->v_current), v_context->v_scope);
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
			return scope;
		}
	}
}

std::unique_ptr<ast::t_node> t_object::f_generate(t_code* a_code)
{
	return std::make_unique<ast::t_quote>(a_code->v_engine, this);
}

void t_object::f_tail(t_engine& a_engine, size_t a_arguments)
{
	f_call(a_engine, a_arguments);
	*a_engine.v_context->v_stack = a_engine.v_used[-1];
	a_engine.v_used = a_engine.v_context->v_stack + 1;
	++a_engine.v_context;
}

std::unique_ptr<ast::t_node> t_symbol::f_generate(t_code* a_code)
{
	auto [outer, binding] = a_code->f_resolve(this);
	return binding->f_generate(a_code, outer);
}

std::unique_ptr<ast::t_node> t_pair::f_generate(t_code* a_code)
{
	auto tail = a_code->v_engine.f_pointer(v_tail);
	return ast::f_generate(v_head, a_code).release()->f_apply(a_code, tail);
}

std::unique_ptr<ast::t_node> t_module::t_variable::f_generate(t_code* a_code, size_t a_outer)
{
	return std::make_unique<ast::t_call>(std::make_unique<ast::t_quote>(a_code->v_engine, a_code->v_engine.f_new<t_get>(this)));
}

std::unique_ptr<ast::t_node> t_module::t_variable::f_generate(t_code* a_code, size_t a_outer, std::unique_ptr<ast::t_node>&& a_expression)
{
	auto p = std::make_unique<ast::t_call>(std::make_unique<ast::t_quote>(a_code->v_engine, a_code->v_engine.f_new<t_set>(this)));
	p->v_arguments.push_back(std::move(a_expression));
	return std::move(p);
}

std::unique_ptr<ast::t_node> t_code::t_lambda::f_generate(t_code* a_code, size_t a_outer)
{
	return std::make_unique<ast::t_lambda>();
}

std::unique_ptr<ast::t_node> t_code::t_local::f_generate(t_code* a_code, size_t a_outer)
{
	return std::make_unique<ast::t_get>(a_outer, v_index);
}

std::unique_ptr<ast::t_node> t_code::t_local::f_generate(t_code* a_code, size_t a_outer, std::unique_ptr<ast::t_node>&& a_expression)
{
	return std::make_unique<ast::t_set>(a_outer, v_index, std::move(a_expression));
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
	if (v_outer) {
		auto pair = require_cast<t_pair>(static_cast<t_object*>(a_source), "must be list");
		for (auto arguments = pair->v_head; arguments;) {
			auto p = require_cast<t_pair>(arguments, "must be list");
			auto symbol = require_cast<t_symbol>(p->v_head, "must be symbol");
			v_bindings.emplace(symbol, new t_local(v_locals.size()));
			v_locals.push_back(symbol);
			arguments = p->v_tail;
		}
		a_source = pair->v_tail;
	}
	v_arguments = v_locals.size();
	auto body = v_engine.f_pointer(a_source);
	ast::t_emit emit{this};
	while (body) {
		auto pair = dynamic_cast<t_pair*>(static_cast<t_object*>(body));
		if (!pair) break;
		auto p = dynamic_cast<t_pair*>(pair->v_head);
		if (!p) break;
		auto symbol = dynamic_cast<t_symbol*>(p->v_head);
		if (!symbol) break;
		auto binder = dynamic_cast<t_binder*>(f_resolve(symbol).second);
		if (!binder) break;
		body = pair->v_tail;
		binder->f_bind(this, p->v_tail)->f_emit(emit, 1, false);
		emit(e_instruction__POP, 1);
	}
	if (body) {
		while (true) {
			auto pair = require_cast<t_pair>(static_cast<t_object*>(body), "must be list");
			body = pair->v_tail;
			ast::f_generate(pair->v_head, this)->f_emit(emit, 1, !body);
			if (!body) break;
			emit(e_instruction__POP, 1);
		}
	} else {
		if (v_outer) throw std::runtime_error("must have an expression");
		emit(e_instruction__INSTANCE, 1)(static_cast<t_object*>(nullptr));
	}
	emit(e_instruction__RETURN, 0);
	for (auto& x : emit.v_labels) {
		auto p = v_instructions.data() + x.v_target;
		for (auto i : x) v_instructions[i] = p;
	}
}

std::unique_ptr<ast::t_node> ast::t_node::f_apply(t_code* a_code, t_object* a_arguments)
{
	auto node = std::make_unique<t_call>(std::unique_ptr<t_node>(this));
	auto arguments = a_code->v_engine.f_pointer(a_arguments);
	while (arguments) {
		auto pair = require_cast<t_pair>(static_cast<t_object*>(arguments), "must be list");
		arguments = pair->v_tail;
		node->v_arguments.push_back(f_generate(pair->v_head, a_code));
	}
	return node;
}

struct t_quote : t_object_of<t_quote>
{
	t_object* v_value;

	t_quote(t_object* a_value) : v_value(a_value)
	{
	}
	virtual void f_scan(t_engine& a_engine)
	{
		v_value = a_engine.f_forward(v_value);
	}
	virtual std::wstring f_string() const
	{
		return L'\'' + lilis::f_string(v_value);
	}
	virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code)
	{
		return std::make_unique<ast::t_quote>(a_code->v_engine, v_value);
	}
};

struct t_define : t_binding, t_binder
{
	virtual std::unique_ptr<ast::t_node> f_bind(t_code* a_code, t_object* a_arguments)
	{
		auto pair = require_cast<t_pair>(a_arguments, "must be list");
		auto symbol = require_cast<t_symbol>(pair->v_head, "must be symbol");
		pair = require_cast<t_pair>(pair->v_tail, "must be list");
		if (pair->v_tail) throw std::runtime_error("must have an expression");
		size_t index = a_code->v_locals.size();
		a_code->v_locals.push_back(symbol);
		a_code->v_bindings.emplace(symbol, new t_code::t_local(index));
		return std::make_unique<ast::t_set>(0, index, ast::f_generate(pair->v_head, a_code));
	}
};

struct t_set : t_binding
{
	struct t_node : ast::t_node
	{
		virtual std::unique_ptr<ast::t_node> f_apply(t_code* a_code, t_object* a_arguments)
		{
			delete this;
			auto pair = require_cast<t_pair>(a_arguments, "must be list");
			auto symbol = require_cast<t_symbol>(pair->v_head, "must be symbol");
			pair = require_cast<t_pair>(pair->v_tail, "must be list");
			if (pair->v_tail) throw std::runtime_error("must have an expression");
			auto [outer, binding] = a_code->f_resolve(symbol);
			if (auto p = dynamic_cast<t_setter*>(binding)) return p->f_generate(a_code, outer, ast::f_generate(pair->v_head, a_code));
			throw std::runtime_error("not setter");
		}
	};

	virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code, size_t a_outer)
	{
		return std::make_unique<t_node>();
	}
};

struct t_if : t_binding
{
	struct t_node : ast::t_node
	{
		virtual std::unique_ptr<ast::t_node> f_apply(t_code* a_code, t_object* a_arguments)
		{
			delete this;
			auto pair = a_code->v_engine.f_pointer(require_cast<t_pair>(a_arguments, "must be list"));
			auto condition = ast::f_generate(pair->v_head, a_code);
			pair = require_cast<t_pair>(pair->v_tail, "must be list");
			auto truee = ast::f_generate(pair->v_head, a_code);
			if (!pair->v_tail) return std::make_unique<ast::t_if>(std::move(condition), std::move(truee), nullptr);
			pair = require_cast<t_pair>(pair->v_tail, "must be list");
			if (pair->v_tail) throw std::runtime_error("must be end");
			return std::make_unique<ast::t_if>(std::move(condition), std::move(truee), ast::f_generate(pair->v_head, a_code));
		}
	};

	virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code, size_t a_outer)
	{
		return std::make_unique<t_node>();
	}
};

struct t_bindable : t_object, t_binding
{
	virtual t_object* f_forward(t_engine& a_engine)
	{
		return this;
	}
	virtual std::unique_ptr<ast::t_node> f_generate(t_code* a_code, size_t a_outer)
	{
		return std::make_unique<ast::t_quote>(a_code->v_engine, this);
	}
};

struct t_eq : t_bindable
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 2) throw std::runtime_error("requires two arguments");
		a_engine.v_used -= 2;
		a_engine.v_used[-1] = a_engine.v_used[0] == a_engine.v_used[1] ? this : nullptr;
	}
};

struct t_cons : t_bindable
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 2) throw std::runtime_error("requires two arguments");
		auto used = a_engine.v_used - 2;
		used[-1] = a_engine.f_new<t_pair>(used[0], used[1]);
		a_engine.v_used = used;
	}
};

struct t_car : t_bindable
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 1) throw std::runtime_error("requires an argument");
		--a_engine.v_used;
		a_engine.v_used[-1] = require_cast<t_pair>(a_engine.v_used[0], "must be list")->v_head;
	}
};

struct t_cdr : t_bindable
{
	virtual void f_call(t_engine& a_engine, size_t a_arguments)
	{
		if (a_arguments != 1) throw std::runtime_error("requires an argument");
		--a_engine.v_used;
		a_engine.v_used[-1] = require_cast<t_pair>(a_engine.v_used[0], "must be list")->v_tail;
	}
};

struct t_print : t_bindable
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
};

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
		void f_call(t_engine& a_engine)
		{
			auto used = a_engine.v_used - 2;
			auto value = used[1];
			auto p = reinterpret_cast<t_object**>(this + 1);
			a_engine.v_used = std::copy_n(p, v_stack, used);
			a_engine.v_context -= v_contexts;
			std::copy_n(reinterpret_cast<t_context*>(p + v_stack), v_contexts, a_engine.v_context);
			for (size_t i = 0; i < v_contexts; ++i) a_engine.v_context[i].v_stack += used - static_cast<t_object**>(nullptr);
			*a_engine.v_used++ = value;
		}
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments != 1) throw std::runtime_error("requires an argument");
			f_call(a_engine);
		}
		virtual void f_tail(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments != 1) throw std::runtime_error("requires an argument");
			a_engine.v_context->v_stack[1] = a_engine.v_used[-1];
			a_engine.v_used = a_engine.v_context->v_stack + 2;
			++a_engine.v_context;
			f_call(a_engine);
		}
	};
	struct t_call : t_bindable
	{
		static void* v_instructions[];

		void f_call(t_engine& a_engine, size_t a_arguments, bool a_tail)
		{
			if (a_arguments != 3) throw std::runtime_error("requires three arguments");
			if (a_tail) {
				a_engine.v_used = std::copy_n(a_engine.v_used - 4, 4, a_engine.v_context->v_stack);
			} else {
				--a_engine.v_context;
				a_engine.v_context->v_stack = a_engine.v_used - 4;
			}
			a_engine.v_context->v_code = nullptr;
			a_engine.v_context->v_current = v_instructions;
			a_engine.v_context->v_scope = nullptr;
			a_engine.v_used[-1]->f_call(a_engine, 0);
		}
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			f_call(a_engine, a_arguments, false);
		}
		virtual void f_tail(t_engine& a_engine, size_t a_arguments)
		{
			f_call(a_engine, a_arguments, true);
		}
	};
	struct t_abort : t_bindable
	{
		void f_abort(t_engine& a_engine, size_t a_arguments)
		{
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
		virtual void f_call(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments < 1) throw std::runtime_error("requires at least one argument");
			f_abort(a_engine, a_arguments);
		}
		virtual void f_tail(t_engine& a_engine, size_t a_arguments)
		{
			if (a_arguments < 1) throw std::runtime_error("requires at least one argument");
			a_engine.v_used = std::copy(a_engine.v_used - a_arguments, a_engine.v_used, a_engine.v_context->v_stack + 1);
			++a_engine.v_context;
			f_abort(a_engine, a_arguments);
		}
	};
	void* t_call::v_instructions[] = {
		reinterpret_cast<void*>(e_instruction__RETURN)
	};
}

void f_print_with_caret(std::FILE* a_out, const char* a_path, long a_position, size_t a_column)
{
	auto file = std::fopen(a_path, "r");
	std::fseek(file, a_position, SEEK_SET);
	std::putc('\t', a_out);
	while (true) {
		int c = std::getc(file);
		if (c == EOF) break;
		std::putc(c, a_out);
		if (c == '\n') break;
	}
	std::fseek(file, a_position, SEEK_SET);
	std::putc('\t', a_out);
	for (size_t i = 1; i < a_column; ++i) {
		int c = std::getc(file);
		std::putc(std::isspace(c) ? c : ' ', a_out);
	}
	std::putc('^', a_out);
	std::putc('\n', a_out);
	std::fclose(file);
}

struct t_at
{
	long v_position;
	size_t v_line;
	size_t v_column;
};

struct t_parser
{
	struct t_error : std::runtime_error
	{
		const t_at v_at;

		t_error(t_parser& a_parser) : std::runtime_error("lexical error"), v_at(a_parser.v_at)
		{
		}
		void f_dump(const char* a_path) const
		{
			std::fprintf(stderr, "at %ls:%" PRIuPTR ":%" PRIuPTR "\n", a_path, static_cast<uintptr_t>(v_at.v_line), static_cast<uintptr_t>(v_at.v_column));
			f_print_with_caret(stderr, a_path, v_at.v_position, v_at.v_column);
		}
	};

	t_engine& v_engine;
	std::function<wint_t()> v_get;
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
			if (v_c == WEOF) throw std::runtime_error("unexpected end of file");
			head = v_engine.f_new<t_pair>(v_engine.f_pointer(f_expression()), nullptr);
			auto tail = v_engine.f_pointer<t_pair>(head);
			while (v_c != L')') {
				if (v_c == WEOF) throw std::runtime_error("unexpected end of file");
				auto p = v_engine.f_new<t_pair>(v_engine.f_pointer(f_expression()), nullptr);
				tail->v_tail = p;
				tail = p;
			}
		}
		f_next();
		return head;
	}

	t_parser(t_engine& a_engine, std::function<wint_t()>&& a_get) : v_engine(a_engine), v_get(std::move(a_get))
	{
		v_c = v_get();
		f_skip();
	}
	t_pair* f_parse()
	{
		if (v_c == WEOF) return nullptr;
		auto head = v_engine.f_pointer(v_engine.f_new<t_pair>(v_engine.f_pointer(f_expression()), nullptr));
		auto tail = v_engine.f_pointer(&*head);
		while (v_c != WEOF) {
			auto p = v_engine.f_new<t_pair>(v_engine.f_pointer(f_expression()), nullptr);
			tail->v_tail = p;
			tail = p;
		}
		return head;
	}
};

t_object* t_parser::f_expression()
{
	switch (v_c) {
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
						throw t_error(*this);
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
			return nullptr;
		} else {
			f_skip();
			return nullptr;
		}
	case L'`':
		f_next();
		return nullptr;
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
					if (!std::iswxdigit(v_c)) throw t_error(*this);
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
						if (v_c >= L'8') throw t_error(*this);
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
					if (!std::iswdigit(v_c)) throw t_error(*this);
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
			} while (v_c != WEOF && !std::iswspace(v_c) && v_c != L')');
			f_skip();
			return v_engine.f_symbol({cs.data(), cs.size()});
		}
	}
}

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
	auto global = engine.f_pointer(engine.f_new<t_holder<t_module>>(engine));
	(*global)->f_register(L"lambda"sv, new t_code::t_lambda());
	(*global)->f_register(L"define"sv, new t_define());
	(*global)->f_register(L"set!"sv, new t_set());
	(*global)->f_register(L"if"sv, new t_if());
	(*global)->f_register(L"eq?"sv, new t_eq());
	(*global)->f_register(L"cons"sv, new t_cons());
	(*global)->f_register(L"car"sv, new t_car());
	(*global)->f_register(L"cdr"sv, new t_cdr());
	(*global)->f_register(L"print"sv, new t_print());
	(*global)->f_register(L"call-with-prompt"sv, new prompt::t_call());
	(*global)->f_register(L"abort-to-prompt"sv, new prompt::t_abort());
	auto module = engine.f_pointer(engine.f_new<t_holder<t_module>>(engine));
	if (argc < 2) {
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
					auto expressions = engine.f_pointer(t_parser(engine, [i = cs.begin()]() mutable
					{
						return *i++;
					}).f_parse());
					if (expressions) {
						auto code = engine.f_pointer(engine.f_new<t_holder<t_code>>(engine, nullptr, module));
						(*code)->v_imports.push_back(global);
						(*code)->v_imports.push_back(module);
						(*code)->f_compile(expressions);
						auto scope = engine.f_pointer(engine.f_run(*code));
						for (auto& x : (*code)->v_bindings) (*module)->insert_or_assign(x.first, x.second->f_export(scope));
						std::printf("%ls\n", f_string(engine.v_used[0]).c_str());
					}
				} catch (std::exception& e) {
					std::fprintf(stderr, "caught: %s\n", e.what());
				}
			}
			if (c == WEOF) break;
		}
	} else {
		try {
			auto expressions = engine.f_pointer<t_pair>(nullptr);
			{
				std::unique_ptr<std::FILE, decltype(&std::fclose)> file(std::fopen(argv[1], "r"), &std::fclose);
				expressions = t_parser(engine, [&]
				{
					return std::getwc(file.get());
				}).f_parse();
			}
			auto code = engine.f_pointer(engine.f_new<t_holder<t_code>>(engine, nullptr, module));
			(*code)->v_imports.push_back(global);
			(*code)->f_compile(expressions);
			engine.f_run(*code);
		} catch (std::exception& e) {
			std::fprintf(stderr, "caught: %s\n", e.what());
			return -1;
		}
	}
	return 0;
}
