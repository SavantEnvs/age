/*
 * catalog-free agtype.c functions: casts to SQL types, ::int/::numeric/::float/::bool/::vertex/::edge/::path typecasts, STARTS WITH / ENDS WITH / CONTAINS, IN, hashing, text->agtype and graphid input.
 *
 * Input: byte 0 selects the function (modulo the table size), the rest is its
 * arguments as NUL-separated agtype text (age_fuzz_args.h). Only functions that
 * work without a system catalog are listed: no get_AGTYPEOID(), syscache or
 * get_fn_expr_argtype() on their paths (those are reached through SQL instead).
 */
#include "age_fuzz_args.h"

extern Datum agtype_to_int8(PG_FUNCTION_ARGS);
extern Datum agtype_to_int4(PG_FUNCTION_ARGS);
extern Datum agtype_to_int2(PG_FUNCTION_ARGS);
extern Datum agtype_to_float8(PG_FUNCTION_ARGS);
extern Datum agtype_to_bool(PG_FUNCTION_ARGS);
extern Datum agtype_to_text(PG_FUNCTION_ARGS);
extern Datum agtype_to_json(PG_FUNCTION_ARGS);
extern Datum agtype_to_graphid(PG_FUNCTION_ARGS);
extern Datum agtype_typecast_int(PG_FUNCTION_ARGS);
extern Datum agtype_typecast_numeric(PG_FUNCTION_ARGS);
extern Datum agtype_typecast_float(PG_FUNCTION_ARGS);
extern Datum agtype_typecast_bool(PG_FUNCTION_ARGS);
extern Datum agtype_typecast_vertex(PG_FUNCTION_ARGS);
extern Datum agtype_typecast_edge(PG_FUNCTION_ARGS);
extern Datum agtype_typecast_path(PG_FUNCTION_ARGS);
extern Datum agtype_string_match_starts_with(PG_FUNCTION_ARGS);
extern Datum agtype_string_match_ends_with(PG_FUNCTION_ARGS);
extern Datum agtype_string_match_contains(PG_FUNCTION_ARGS);
extern Datum agtype_in_operator(PG_FUNCTION_ARGS);
extern Datum agtype_hash_cmp(PG_FUNCTION_ARGS);
extern Datum text_to_agtype(PG_FUNCTION_ARGS);
extern Datum graphid_in(PG_FUNCTION_ARGS);

static const age_fuzz_fn fns[] = {
	{"agtype_to_int8", agtype_to_int8, 1, {ARG_AGTYPE}, false},
	{"agtype_to_int4", agtype_to_int4, 1, {ARG_AGTYPE}, false},
	{"agtype_to_int2", agtype_to_int2, 1, {ARG_AGTYPE}, false},
	{"agtype_to_float8", agtype_to_float8, 1, {ARG_AGTYPE}, false},
	{"agtype_to_bool", agtype_to_bool, 1, {ARG_AGTYPE}, false},
	{"agtype_to_text", agtype_to_text, 1, {ARG_AGTYPE}, false},
	{"agtype_to_json", agtype_to_json, 1, {ARG_AGTYPE}, false},
	{"agtype_to_graphid", agtype_to_graphid, 1, {ARG_AGTYPE}, false},
	{"agtype_typecast_int", agtype_typecast_int, 1, {ARG_AGTYPE}, true},
	{"agtype_typecast_numeric", agtype_typecast_numeric, 1, {ARG_AGTYPE}, true},
	{"agtype_typecast_float", agtype_typecast_float, 1, {ARG_AGTYPE}, true},
	{"agtype_typecast_bool", agtype_typecast_bool, 1, {ARG_AGTYPE}, true},
	{"agtype_typecast_vertex", agtype_typecast_vertex, 1, {ARG_AGTYPE}, true},
	{"agtype_typecast_edge", agtype_typecast_edge, 1, {ARG_AGTYPE}, true},
	{"agtype_typecast_path", agtype_typecast_path, 1, {ARG_AGTYPE}, true},
	{"agtype_string_match_starts_with", agtype_string_match_starts_with, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_string_match_ends_with", agtype_string_match_ends_with, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_string_match_contains", agtype_string_match_contains, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_in_operator", agtype_in_operator, 2, {ARG_AGTYPE, ARG_AGTYPE}, true},
	{"agtype_hash_cmp", agtype_hash_cmp, 1, {ARG_AGTYPE}, false},
	{"text_to_agtype", text_to_agtype, 1, {ARG_TEXT}, true},
	{"graphid_in", graphid_in, 1, {ARG_CSTRING}, false},
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
