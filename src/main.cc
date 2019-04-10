#include "parser.h"
#include "builtins.h"
#include <fstream>
#include <cstring>

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
	if (argc < 2) {
		std::wcerr << L"usage: " << argv[0] << " [options] <script> ..." << std::endl;
		return -1;
	}
	using namespace lilis;
	t_engine engine(debug, verbose);
	try {
		auto path = std::filesystem::absolute(argv[1]);
		if (auto expressions = engine.f_pointer(engine.f_parse(path))) {
			f_define_builtins(**engine.v_global);
			engine.f_run(engine.f_new<t_holder<t_module>>(engine, path), expressions);
		}
	} catch (std::exception& e) {
		std::wcerr << L"caught: " << e.what() << std::endl;
		if (auto p = dynamic_cast<t_error*>(&e)) p->f_dump();
		return -1;
	}
	return 0;
}
