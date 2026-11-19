#!/usr/bin/env bash
#
# mayhem/test.sh — runs Apache AGE's own regression suite (pg_regress, the REGRESS list of the upstream
# Makefile) against the age build that mayhem/build.sh installed into mayhem/install-test (normal flags),
# loaded into a temporary instance of the pinned PostgreSQL under $PG_PREFIX. Each regression test diffs
# psql's output against regress/expected/<test>.out, so a neutered age fails it. Emits a CTRF report.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"
: "${SRC:=/mayhem}"
: "${PG_PREFIX:=/opt/toolchains/deps/postgresql-18.7}"
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

fail_setup() { echo "test.sh: $*" >&2; emit_ctrf pg_regress 0 1; exit 1; }

INST="$SRC/mayhem/install-test$PG_PREFIX"
PG_REGRESS="$PG_PREFIX/lib/pgxs/src/test/regress/pg_regress"
[ -x "$PG_REGRESS" ] || fail_setup "missing $PG_REGRESS (built by mayhem/Dockerfile)"
[ -f "$INST/lib/age.so" ] && [ -f "$INST/share/extension/age.control" ] \
  || fail_setup "missing the age test install under $INST (built by mayhem/build.sh)"

# Upstream's REGRESS list, minus age_upgrade (needs git history to synthesize an upgrade script; it is
# only scheduled upstream when that is available) and drop (runs last; kept). age_load copies its CSV
# fixtures from regress/ to /tmp/age itself.
TESTS="scan graphid agtype agtype_hash_cmp agehash catalog cypher expr cypher_create cypher_match
  cypher_unwind cypher_set cypher_remove cypher_delete cypher_with cypher_vle age_shortest_path
  cypher_union cypher_call cypher_merge cypher_subquery age_global_graph age_load index analyze
  graph_generation name_validation jsonb_operators list_comprehension predicate_functions
  pattern_expression age_reduce map_projection direct_field_access generated_columns security rls_vle
  reserved_keyword_alias agtype_jsonb_cast containment_selectivity subgraph extension_security drop"

OUTDIR="$SRC/mayhem/regress-out"
rm -rf "$OUTDIR"; mkdir -p "$OUTDIR"
# The extension is found in the test install, not the read-only PostgreSQL prefix (PG 18 search paths).
{ cat regress/age_regression.conf
  echo "extension_control_path = '\$system:$INST/share'"
  echo "dynamic_library_path = '\$libdir:$INST/lib'"
} > "$OUTDIR/age_regression.conf"

"$PG_REGRESS" --bindir="$PG_PREFIX/bin" --load-extension=age --inputdir="$SRC/regress" \
  --expecteddir="$SRC/regress" --outputdir="$OUTDIR" --temp-instance="$OUTDIR/instance" \
  --port=61958 --dbname=contrib_regression --encoding=UTF-8 --temp-config "$OUTDIR/age_regression.conf" $TESTS > "$OUTDIR/run.log" 2>&1
cat "$OUTDIR/run.log"

passed=$(grep -cE '^ok [0-9]+ ' "$OUTDIR/run.log")
failed=$(grep -cE '^not ok [0-9]+ ' "$OUTDIR/run.log")
ntests=$(echo $TESTS | wc -w)
# tests pg_regress never reported (instance failed to start, crash) count as failed
missing=$(( ntests - passed - failed )); [ "$missing" -gt 0 ] && failed=$(( failed + missing ))
[ "$failed" -gt 0 ] && [ -f "$OUTDIR/regression.diffs" ] && head -100 "$OUTDIR/regression.diffs"
emit_ctrf pg_regress "$passed" "$failed"
