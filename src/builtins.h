#ifndef LILIS__BUILTINS_H
#define LILIS__BUILTINS_H

#include "engine.h"

namespace lilis
{

t_object* f_unquasiquote(t_code& a_code, const std::shared_ptr<t_location>& a_location, t_object* a_value);
void f_define_builtins(t_module& a_module);

}

#endif
