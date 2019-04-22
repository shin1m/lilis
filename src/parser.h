#ifndef LILIS__PARSER_H
#define LILIS__PARSER_H

#include "code.h"

namespace lilis
{

template<typename T_get, typename T_pair, typename T_location>
struct t_parser
{
	t_engine& v_engine;
	T_get v_get;
	T_pair v_pair;
	T_location v_location;
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
	void f_throw [[noreturn]] (std::wstring&& a_message) const
	{
		t_error error{std::move(a_message)};
		error.v_backtrace.push_back(v_location(v_at));
		throw error;
	}
	t_object* f_expression();
	auto f_head()
	{
		auto at = v_at;
		return v_pair(f_expression(), at);
	}
	void f_push(gc::t_pointer<std::remove_pointer_t<decltype(v_pair(nullptr, {}))>>& a_p)
	{
		auto at = a_p->v_where_tail = v_at;
		auto p = v_pair(f_expression(), at);
		a_p->v_tail = p;
		a_p = p;
	}

	t_parser(t_engine& a_engine, T_get&& a_get, T_pair&& a_pair, T_location&& a_location) : v_engine(a_engine), v_get(std::forward<T_get>(a_get)), v_pair(std::forward<T_pair>(a_pair)), v_location(std::forward<T_location>(a_location))
	{
		v_c = v_get();
		f_skip();
	}
	decltype(v_pair(nullptr, {})) operator()()
	{
		if (v_c == WEOF) return nullptr;
		auto list = v_engine.f_pointer(f_head());
		auto last = v_engine.f_pointer(list.v_value);
		while (v_c != WEOF) f_push(last);
		last->v_where_tail = v_at;
		return list.v_value;
	}
};

template<typename T_get, typename T_pair, typename T_location>
t_object* t_parser<T_get, T_pair, T_location>::f_expression()
{
	switch (v_c) {
	case WEOF:
		f_throw(L"unexpected end of file"s);
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
						f_throw(L"lexical error"s);
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
		{
			f_next();
			auto list = v_engine.f_pointer<std::remove_pointer_t<decltype(v_pair(nullptr, {}))>>(nullptr);
			if (v_c != L')') {
				list = f_head();
				for (auto last = v_engine.f_pointer(list.v_value);;) {
					if (v_c == L')') {
						last->v_where_tail = v_at;
						break;
					} else if (v_c == L'.') {
						f_next();
						last->v_where_tail = v_at;
						last->v_tail = f_expression();
						if (v_c != L')') f_throw(L"must be ')'"s);
						break;
					}
					f_push(last);
				}
			}
			f_next();
			return list;
		}
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
					if (!std::iswxdigit(v_c)) f_throw(L"lexical error"s);
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
						if (v_c >= L'8') f_throw(L"lexical error"s);
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
					if (!std::iswdigit(v_c)) f_throw(L"lexical error"s);
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

template<typename T>
struct t_parsed_pair : t_object_of<t_parsed_pair<T>, t_pair>
{
	T v_source;
	t_at v_where_head;
	t_at v_where_tail;

	t_parsed_pair(t_object* a_head, const T& a_source, const t_at& a_where_head) : t_object_of<t_parsed_pair, t_pair>(a_head, nullptr), v_source(a_source), v_where_head(a_where_head)
	{
	}
};

}

#endif
