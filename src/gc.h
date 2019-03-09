#ifndef LILIS__GC_H
#define LILIS__GC_H

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace lilis::gc
{

struct t_collector;

struct t_object
{
	virtual size_t f_size() const
	{
		throw std::runtime_error("invalid");
	}
	virtual t_object* f_forward(t_collector& a_collector) = 0;
	virtual void f_scan(t_collector& a_collector)
	{
	}
	virtual void f_destruct(t_collector& a_collector)
	{
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
	virtual void f_scan(t_collector& a_collector) = 0;
};

template<typename T>
struct t_pointer : t_root
{
	T* v_value;

	t_pointer(t_collector& a_collector, T* a_value) : t_root(a_collector), v_value(a_value)
	{
	}
	t_pointer& operator=(T* a_value)
	{
		v_value = a_value;
		return *this;
	}
	virtual void f_scan(t_collector& a_collector);
	operator T*() const
	{
		return v_value;
	}
	T* operator->() const
	{
		return v_value;
	}
};

struct t_collector : t_root
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
		virtual t_object* f_forward(t_collector& a_collector)
		{
			return v_moved;
		}
	};

	size_t v_size = 1024;
	std::unique_ptr<char[]> v_heap0{new char[v_size]};
	std::unique_ptr<char[]> v_heap1{new char[v_size]};
	char* v_head = v_heap0.get();
	char* v_tail = v_head + v_size;
	bool v_debug;
	bool v_verbose;

	t_collector(bool a_debug, bool a_verbose) : v_debug(a_debug), v_verbose(a_verbose)
	{
	}
	template<typename T>
	T* f_move(T* a_p)
	{
		size_t n = a_p->f_size();
		auto p = v_tail;
		v_tail = std::copy_n(reinterpret_cast<char*>(a_p), n, p);
		new(a_p) t_forward(reinterpret_cast<t_object*>(p), n);
		return reinterpret_cast<T*>(p);
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
			if (v_verbose) std::cerr << "gc collecting..." <<std::endl;
			f_compact();
			for (auto q = v_heap1.get(); q != p;) {
				auto p = reinterpret_cast<t_object*>(q);
				q += p->f_size();
				p->f_destruct(*this);
			}
			if (v_verbose) std::cerr << "gc done: " << v_tail - v_head << " bytes free" << std::endl;
			while (true) {
				p = v_head;
				v_head += a_n;
				if (v_head <= v_tail) break;
				if (v_verbose) std::cerr << "gc expanding..." << std::endl;
				v_size *= 2;
				v_heap1.reset(new char[v_size]);
				f_compact();
				v_heap1.reset(new char[v_size]);
				if (v_verbose) std::cerr << "gc done: " << v_tail - v_head << " bytes free" << std::endl;
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
	virtual void f_scan(t_collector& a_collector)
	{
	}
};

template<typename T>
void t_pointer<T>::f_scan(t_collector& a_collector)
{
	v_value = a_collector.f_forward(v_value);
}

}

#endif
