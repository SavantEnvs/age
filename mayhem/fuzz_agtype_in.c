/*
 * agtype text and binary input (agtype_parser.c, agtype_util.c, agtype.c).
 *
 * The input is the text of an agtype literal, as a client sends it for a value
 * of type agtype ('{"a": 1}::vertex', '[1, 2.5::numeric]', ...). It is parsed
 * with agtype_in; a value that parses is printed with agtype_out, the output
 * is parsed again, and the value goes through agtype_send. The same bytes,
 * behind the binary format's version byte, are also fed to agtype_recv.
 */
#include "age_fuzz_args.h"
#include "lib/stringinfo.h"

extern Datum agtype_send(PG_FUNCTION_ARGS);
extern Datum agtype_recv(PG_FUNCTION_ARGS);

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
	age_fuzz_init();
	return 0;
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	PGFUZZ_BEGIN();
	char	   *s = pgfuzz_cstring(data, size);

	if (!pgfuzz_valid_text(s, size))
		pgfuzz_rc = FUZZ_REJECT;
	else
	{
		PG_TRY();
		{
			Datum		v = DirectFunctionCall1(agtype_in, CStringGetDatum(s));
			char	   *out = DatumGetCString(DirectFunctionCall1(agtype_out, v));

			(void) DirectFunctionCall1(agtype_in, CStringGetDatum(out));
			(void) DirectFunctionCall1(agtype_send, v);
		}
		PGFUZZ_CATCH(0);

		PG_TRY();
		{
			/* binary format: a version byte (1) followed by the agtype text */
			StringInfoData buf;

			initStringInfo(&buf);
			appendStringInfoChar(&buf, 1);
			appendBinaryStringInfo(&buf, s, (int) size);
			(void) DirectFunctionCall1(agtype_recv, PointerGetDatum(&buf));
		}
		PGFUZZ_CATCH(0);
	}
	PGFUZZ_END();
}
