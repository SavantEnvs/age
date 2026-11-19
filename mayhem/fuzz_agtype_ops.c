/*
 * agtype operators (agtype_ops.c): arithmetic, comparison, ?, ?|, ?&, @>, <@, ||, #>, #>>.
 *
 * Input: byte 0 selects the function (modulo the table size), the rest is its
 * arguments as NUL-separated agtype text (age_fuzz_args.h). Only functions that
 * work without a system catalog are listed: no get_AGTYPEOID(), syscache or
 * get_fn_expr_argtype() on their paths (those are reached through SQL instead).
 */
#include "age_fuzz_args.h"

extern Datum agtype_add(PG_FUNCTION_ARGS);
extern Datum agtype_sub(PG_FUNCTION_ARGS);
extern Datum agtype_mul(PG_FUNCTION_ARGS);
extern Datum agtype_div(PG_FUNCTION_ARGS);
extern Datum agtype_mod(PG_FUNCTION_ARGS);
extern Datum agtype_pow(PG_FUNCTION_ARGS);
extern Datum agtype_concat(PG_FUNCTION_ARGS);
extern Datum agtype_extract_path(PG_FUNCTION_ARGS);
extern Datum agtype_extract_path_text(PG_FUNCTION_ARGS);
extern Datum agtype_neg(PG_FUNCTION_ARGS);
extern Datum agtype_eq(PG_FUNCTION_ARGS);
extern Datum agtype_ne(PG_FUNCTION_ARGS);
extern Datum agtype_lt(PG_FUNCTION_ARGS);
extern Datum agtype_gt(PG_FUNCTION_ARGS);
extern Datum agtype_le(PG_FUNCTION_ARGS);
extern Datum agtype_ge(PG_FUNCTION_ARGS);
extern Datum agtype_exists_agtype(PG_FUNCTION_ARGS);
extern Datum agtype_exists_any_agtype(PG_FUNCTION_ARGS);
extern Datum agtype_exists_all_agtype(PG_FUNCTION_ARGS);
extern Datum agtype_contains(PG_FUNCTION_ARGS);
extern Datum agtype_contained_by(PG_FUNCTION_ARGS);
extern Datum agtype_contains_top_level(PG_FUNCTION_ARGS);
extern Datum agtype_contained_by_top_level(PG_FUNCTION_ARGS);
extern Datum agtype_exists(PG_FUNCTION_ARGS);

static const age_fuzz_fn fns[] = {
	{"agtype_add", agtype_add, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_sub", agtype_sub, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_mul", agtype_mul, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_div", agtype_div, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_mod", agtype_mod, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_pow", agtype_pow, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_concat", agtype_concat, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_extract_path", agtype_extract_path, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_extract_path_text", agtype_extract_path_text, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_neg", agtype_neg, 1, {ARG_AGTYPE}, true},
	{"agtype_eq", agtype_eq, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_ne", agtype_ne, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_lt", agtype_lt, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_gt", agtype_gt, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_le", agtype_le, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_ge", agtype_ge, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_exists_agtype", agtype_exists_agtype, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_exists_any_agtype", agtype_exists_any_agtype, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_exists_all_agtype", agtype_exists_all_agtype, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_contains", agtype_contains, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_contained_by", agtype_contained_by, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_contains_top_level", agtype_contains_top_level, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_contained_by_top_level", agtype_contained_by_top_level, 2, {ARG_AGTYPE, ARG_AGTYPE}, false},
	{"agtype_exists", agtype_exists, 2, {ARG_AGTYPE, ARG_TEXT}, false},
};

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
	age_fuzz_init();
	return 0;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const age_fuzz_fn *f;
	char	   *args[3];

	if (size < 1)
		return FUZZ_REJECT;
	f = &fns[data[0] % lengthof(fns)];

	{
		PGFUZZ_BEGIN();
		if (!age_fuzz_split(data + 1, size - 1, f->nargs, args))
			pgfuzz_rc = FUZZ_REJECT;
		else
		{
			PG_TRY();
			{
				(void) age_fuzz_call(f, args);
			}
			PGFUZZ_CATCH(0);
		}
		PGFUZZ_END();
	}
}
