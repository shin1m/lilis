#ifndef LILIS__BUILTINS_H
#define LILIS__BUILTINS_H

#include "engine.h"

namespace lilis
{

struct t_static : t_object
{
	virtual t_object* f_forward(gc::t_collector& a_collector);
};

extern struct t_define : t_static
{
	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments);
} v_define;

extern struct t_set : t_static
{
	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments);
} v_set;

extern struct t_macro : t_static
{
	virtual t_object* f_apply(t_code* a_code, t_object* a_arguments);
} v_macro;

t_object* f_unquasiquote(t_code* a_code, t_object* a_value);
void f_define_builtins(t_module& a_module);

}

#endif
