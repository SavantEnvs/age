/*
 * The openCypher scanner and grammar (ag_scanner.l, cypher_gram.y,
 * cypher_parser.c), as cypher() runs them on the query string a client passes
 * in SELECT * FROM cypher('graph', $$ <query> $$). A query that parses is
 * serialized with nodeToString, which runs age's extensible-node output
 * functions (cypher_outfuncs.c) over the whole parse tree.
 */
#include "pg_fuzz_common.h"
#include "nodes/pg_list.h"
#include "nodes/nodes.h"

extern List *parse_cypher(const char *s);
extern void register_ag_nodes(void);

int
LLVMFuzzerInitialize(int *argc, char ***argv)
{
	pgfuzz_init();
	/* what age's _PG_init() does first: register the Cypher parse-node types */
	register_ag_nodes();
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
			List	   *stmt = parse_cypher(s);

			if (stmt != NIL)
				(void) nodeToString(stmt);
		}
		PGFUZZ_CATCH(0);
	}
	PGFUZZ_END();
}
