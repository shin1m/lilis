#ifndef LILIS__PARSER_H
#define LILIS__PARSER_H

#include "engine.h"

namespace lilis
{

template<typename T_seek, typename T_get>
void f_print_with_caret(T_seek a_seek, T_get a_get, std::ostream& a_out, size_t a_column)
{
	a_seek();
	a_out << L'\t';
	while (true) {
		wint_t c = a_get();
		if (c == WEOF || c == L'\n') break;
		a_out << c;
	}
	a_out << std::endl;
	a_seek();
	a_out << L'\t';
	for (size_t i = 1; i < a_column; ++i) {
		wint_t c = a_get();
		a_out << (std::iswspace(c) ? c : L' ');
	}
	a_out << L'^' << std::endl;
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
		std::cerr << "at " << a_path << ':' << v_at.v_line << ':' << v_at.v_column << std::endl;
		f_print_with_caret([&]
		{
			a_seek(v_at.v_position);
		}, a_get, std::cerr, v_at.v_column);
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
		auto list = v_engine.f_pointer<t_pair>(nullptr);
		if (v_c != L')') {
			list = v_engine.f_new<t_pair>(v_engine.f_pointer(f_expression()), nullptr);
			auto last = v_engine.f_pointer<t_pair>(list);
			while (v_c != L')') {
				if (v_c == L'.') {
					f_next();
					last->v_tail = f_expression();
					if (v_c != L')') throw t_error("must be ')'", v_at);
					break;
				}
				f_push(v_engine, last, f_expression());
			}
		}
		f_next();
		return list;
	}

	t_parser(t_engine& a_engine, T_get&& a_get) : v_engine(a_engine), v_get(std::forward<T_get>(a_get))
	{
		v_c = v_get();
		f_skip();
	}
	t_pair* operator()()
	{
		if (v_c == WEOF) return nullptr;
		auto list = v_engine.f_pointer(v_engine.f_new<t_pair>(v_engine.f_pointer(f_expression()), nullptr));
		auto last = v_engine.f_pointer<t_pair>(list);
		while (v_c != WEOF) f_push(v_engine, last, f_expression());
		return list;
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

}

#endif
