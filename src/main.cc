#include "code.h"
#include "parser.h"
#include "builtins.h"
#include <fstream>
#include <cstring>

namespace
{

using namespace lilis;

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
	f_define_builtins(**engine.v_global);
	if (argc < 2) {
		auto module = engine.f_pointer(engine.f_new<t_holder<t_module>>(engine, ""sv));
		t_and_export v_macro_and_export(&v_macro);
		(*module)->f_register(L"define-macro"sv, &v_macro_and_export);
		t_and_export v_define_and_export(&v_define);
		(*module)->f_register(L"define"sv, &v_define_and_export);
		t_and_export v_set_and_export(&v_set);
		(*module)->f_register(L"set!"sv, &v_set_and_export);
		while (std::wcin) {
			std::wcout << L"> ";
			std::wstring cs;
			std::getline(std::wcin, cs);
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
						std::wcout << f_string(engine.v_used[0]) << std::endl;
					}
				} catch (std::exception& e) {
					std::cerr << "caught: " << e.what() << std::endl;
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
		}
	} else {
		auto path = std::filesystem::absolute(argv[1]);
		auto module = engine.f_pointer(engine.f_new<t_holder<t_module>>(engine, path));
		try {
			engine.f_run(module, engine.f_parse(path));
		} catch (std::exception& e) {
			std::cerr << "caught: " << e.what() << std::endl;
			if (auto p = dynamic_cast<t_error*>(&e)) {
				std::wfilebuf fb;
				fb.open(path, std::ios_base::in);
				p->f_dump(path.c_str(), [&](long a_position)
				{
					fb.pubseekpos(a_position);
				}, [&]
				{
					return fb.sbumpc();
				});
			}
			return -1;
		}
	}
	return 0;
}
