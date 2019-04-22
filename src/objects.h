#ifndef LILIS__OBJECTS_H
#define LILIS__OBJECTS_H

#include "gc.h"
#include <functional>
#include <map>
#include <string>

namespace lilis
{

using namespace std::literals;

struct t_engine;
struct t_code;
struct t_location;
struct t_pair;
struct t_emit;

struct t_dump
{
	std::function<void(std::wstring_view)> v_put;
	std::function<void(const t_pair*)> v_head;
	std::function<void(const t_pair*)> v_tail;

	const t_dump& operator<<(std::wstring_view a_x) const
	{
		v_put(a_x);
		return *this;
	}
	const t_dump& operator<<(wchar_t a_x) const
	{
		return *this << std::wstring_view(&a_x, 1);
	}
};

struct t_object : gc::t_object
{
	virtual t_object* f_render(t_code& a_code, const std::shared_ptr<t_location>& a_location);
	virtual t_object* f_apply(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_pair* a_pair);
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail);
	virtual void f_call(t_engine& a_engine, size_t a_arguments);
	virtual void f_dump(const t_dump& a_dump) const;
};

template<typename T, typename T_base = t_object>
struct t_object_of : T_base
{
	using T_base::T_base;
	virtual size_t f_size() const
	{
		return std::max(sizeof(T), sizeof(gc::t_collector::t_forward));
	}
	virtual t_object* f_forward(gc::t_collector& a_collector)
	{
		return a_collector.f_move(this);
	}
};

struct t_symbol : t_object_of<t_symbol>
{
	std::map<std::wstring, t_symbol*, std::less<>>::iterator v_entry;

	t_symbol(std::map<std::wstring, t_symbol*, std::less<>>::iterator a_entry) : v_entry(a_entry)
	{
	}
	virtual void f_scan(gc::t_collector& a_collector);
	virtual void f_destruct(gc::t_collector& a_collector);
	virtual t_object* f_render(t_code& a_code, const std::shared_ptr<t_location>& a_location);
	virtual void f_dump(const t_dump& a_dump) const;
};

struct t_pair : t_object_of<t_pair>
{
	t_object* v_head;
	t_object* v_tail;

	t_pair(t_object* a_head, t_object* a_tail) : v_head(a_head), v_tail(a_tail)
	{
	}
	virtual void f_scan(gc::t_collector& a_collector);
	virtual t_object* f_render(t_code& a_code, const std::shared_ptr<t_location>& a_location);
	virtual void f_dump(const t_dump& a_dump) const;
};

template<typename T_base, typename T>
struct t_with_value : T_base
{
	using t_base = t_with_value<T_base, T>;

	T* v_value;

	t_with_value(T* a_value) : v_value(a_value)
	{
	}
	virtual void f_scan(gc::t_collector& a_collector)
	{
		v_value = a_collector.f_forward(v_value);
	}
};

struct t_quote : t_with_value<t_object_of<t_quote>, t_object>
{
	using t_base::t_base;
	virtual void f_emit(t_emit& a_emit, size_t a_stack, bool a_tail);
	virtual void f_dump(const t_dump& a_dump) const;
};

struct t_unquote : t_with_value<t_object_of<t_unquote>, t_object>
{
	using t_base::t_base;
	virtual void f_dump(const t_dump& a_dump) const;
};

struct t_unquote_splicing : t_unquote
{
	using t_unquote::t_unquote;
	virtual void f_dump(const t_dump& a_dump) const;
};

struct t_quasiquote : t_with_value<t_object_of<t_quasiquote>, t_object>
{
	using t_base::t_base;
	virtual t_object* f_render(t_code& a_code, const std::shared_ptr<t_location>& a_location);
	virtual void f_dump(const t_dump& a_dump) const;
};

inline const t_dump& operator<<(const t_dump& a_dump, t_object* a_value)
{
	if (!a_value) return a_dump << L"()"sv;
	a_value->f_dump(a_dump);
	return a_dump;
}

inline void f_push(gc::t_collector& a_collector, gc::t_pointer<t_pair>& a_p, t_object* a_value)
{
	auto p = a_collector.f_new<t_pair>(a_collector.f_pointer(a_value), nullptr);
	a_p->v_tail = p;
	a_p = p;
}

}

#endif
