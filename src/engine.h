#ifndef LILIS__ENGINE_H
#define LILIS__ENGINE_H

#include "objects.h"
#include <filesystem>

namespace lilis
{

struct t_scope;
struct t_module;

template<typename T>
struct t_holder : t_object_of<t_holder<T>>
{
	T* v_holdee;

	template<typename... T_an>
	t_holder(t_engine& a_engine, T_an&&... a_an) : v_holdee(new T(a_engine, this, std::forward<T_an>(a_an)...))
	{
	}
	virtual void f_scan(gc::t_collector& a_collector)
	{
		v_holdee->f_scan();
	}
	virtual void f_destruct(gc::t_collector& a_collector)
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

struct t_frame
{
	t_holder<t_code>* v_code;
	void** v_current;
	t_scope* v_scope;
	t_object** v_stack;

	void f_scan(gc::t_collector& a_collector)
	{
		v_code = a_collector.f_forward(v_code);
		v_scope = a_collector.f_forward(v_scope);
	}
};

struct t_engine : gc::t_collector
{
	static constexpr size_t V_STACK = 1024;
	static constexpr size_t V_FRAMES = 256;

	std::unique_ptr<t_object*[]> v_stack{new t_object*[V_STACK]};
	std::unique_ptr<t_frame[]> v_frames{new t_frame[V_FRAMES]};
	t_object** v_used = v_stack.get();
	t_frame* v_frame = v_frames.get() + V_FRAMES;
	std::map<std::wstring, t_symbol*, std::less<>> v_symbols;
	t_holder<t_module>* v_global = nullptr;
	std::map<std::wstring, t_holder<t_module>*, std::less<>> v_modules;
	bool v_debug;
	bool v_verbose;

	t_engine(bool a_debug, bool a_verbose) : gc::t_collector(a_debug, a_verbose)
	{
		v_global = f_new<t_holder<t_module>>(*this, std::filesystem::path{});
	}
	virtual void f_scan(t_collector& a_collector);
	t_symbol* f_symbol(std::wstring_view a_name);
	size_t f_expand(size_t a_arguments);
	void f_run(t_code* a_code, t_object* a_arguments);
	t_pair* f_parse(const std::filesystem::path& a_path);
	void f_run(t_holder<t_module>* a_module, t_pair* a_expressions);
	t_holder<t_module>* f_module(const std::filesystem::path& a_path, std::wstring_view a_name);
};

}

#endif
