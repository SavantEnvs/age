/*
 * Argument decoding shared by the agtype function-table harnesses.
 *
 * Input layout: byte 0 selects an entry of the harness's function table; the
 * rest is split on NUL bytes into the function's agtype arguments, each given
 * as agtype text and converted with agtype_in (which is how every SQL value of
 * type agtype is produced from client text).
 */
#ifndef AGE_FUZZ_ARGS_H
#define AGE_FUZZ_ARGS_H

#include "pg_fuzz_common.h"
#include "fmgr.h"
#include "utils/builtins.h"
#include "catalog/pg_type_d.h"
#include "nodes/makefuncs.h"
#include "nodes/primnodes.h"

typedef enum
{
	ARG_AGTYPE,
	ARG_TEXT,
	ARG_CSTRING
} age_arg_kind;

typedef struct
{
	const char *name;
	PGFunction	fn;
	int			nargs;
	age_arg_kind kinds[3];
	bool		ret_agtype;		/* result is agtype: also print it with agtype_out */
} age_fuzz_fn;

/*
 * The OIDs of age's types, cached by agtype.c/graphid.c after a syscache
 * lookup; build.sh makes them global so they can be preset (there is no
 * catalog to look them up in). Any distinct user-range OIDs do.
 */
extern Oid	g_AGTYPEOID, g_AGTYPEARRAYOID, g_VERTEXOID, g_EDGEOID;
extern Oid	g_GRAPHIDOID, g_GRAPHIDARRAYOID;

static void
age_fuzz_init(void)
{
	pgfuzz_init();
	g_GRAPHIDOID = 16400;
	g_GRAPHIDARRAYOID = 16401;
	g_AGTYPEOID = 16402;
	g_AGTYPEARRAYOID = 16403;
	g_VERTEXOID = 16404;
	g_EDGEOID = 16405;
}

extern Datum agtype_in(PG_FUNCTION_ARGS);
extern Datum agtype_out(PG_FUNCTION_ARGS);

/* Splits data[1..] on NUL into exactly nargs strings; returns false if the count differs. */
static bool
age_fuzz_split(const uint8_t *data, size_t size, int nargs, char **out)
{
	char	   *buf = pgfuzz_cstring(data, size);
	char	   *end = buf + size;
	char	   *p = buf;
	int			n = 0;

	while (n < nargs)
	{
		size_t		len = strnlen(p, end - p);

		if (!pg_verify_mbstr(PG_UTF8, p, (int) len, true))
			return false;
		out[n++] = p;
		p += len;
		if (p >= end)
			break;
		p++;					/* skip the NUL separator */
	}
	return n == nargs && p >= end;
}

/* Calls fn on the decoded arguments with the default (C) collation; errors propagate. */
static Datum
age_fuzz_call(const age_fuzz_fn *f, char **args)
{
	LOCAL_FCINFO(fcinfo, 3);
	FmgrInfo   *flinfo = palloc0(sizeof(FmgrInfo));
	FuncExpr   *expr = makeNode(FuncExpr);
	Datum		result;
	int			i;

	/*
	 * The call expression the planner would attach, so get_fn_expr_argtype()
	 * reports each argument's SQL type, as it does for every SQL-level call.
	 */
	for (i = 0; i < f->nargs; i++)
	{
		Oid			type = f->kinds[i] == ARG_CSTRING ? CSTRINGOID :
			f->kinds[i] == ARG_TEXT ? TEXTOID : g_AGTYPEOID;

		expr->args = lappend(expr->args,
							 makeConst(type, -1, InvalidOid, -1, (Datum) 0, true, false));
	}
	expr->funcvariadic = false;
	flinfo->fn_addr = f->fn;
	flinfo->fn_oid = InvalidOid;
	flinfo->fn_nargs = f->nargs;
	flinfo->fn_strict = true;
	flinfo->fn_mcxt = CurrentMemoryContext;
	flinfo->fn_expr = (Node *) expr;

	InitFunctionCallInfoData(*fcinfo, flinfo, f->nargs, DEFAULT_COLLATION_OID, NULL, NULL);
	for (i = 0; i < f->nargs; i++)
	{
		if (f->kinds[i] == ARG_CSTRING)
			fcinfo->args[i].value = CStringGetDatum(args[i]);
		else if (f->kinds[i] == ARG_TEXT)
			fcinfo->args[i].value = PointerGetDatum(cstring_to_text(args[i]));
		else
			fcinfo->args[i].value = DirectFunctionCall1(agtype_in, CStringGetDatum(args[i]));
		fcinfo->args[i].isnull = false;
	}
	result = f->fn(fcinfo);
	if (!fcinfo->isnull && f->ret_agtype)
		(void) DirectFunctionCall1(agtype_out, result);
	return fcinfo->isnull ? (Datum) 0 : result;
}

#endif
