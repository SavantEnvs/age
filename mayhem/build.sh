#!/usr/bin/env bash
#
# mayhem/build.sh — builds Apache AGE (a PostgreSQL extension, PGXS) twice, against the pinned
# PostgreSQL the Dockerfile installed under $PG_PREFIX (/opt/toolchains/deps/postgresql-<ver>):
#   1) normal flags -> age.so + SQL/control files installed into mayhem/install-test (DESTDIR,
#      gitignored), which mayhem/test.sh loads into a temporary server to run age's regression suite;
#   2) sanitized + SanCov-instrumented objects, linked with the backend archive
#      ($PG_PREFIX/lib/libpgbackend_fuzz.a) into the in-process libFuzzer harnesses /mayhem/fuzz_*
#      and their /mayhem/fuzz_*-standalone reproducers.
# Both builds happen in the source tree (PGXS has no out-of-tree mode), so each starts with `make clean`;
# re-running is safe and needs no network.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${PG_PREFIX:=/opt/toolchains/deps/postgresql-18.7}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

SRC="${SRC:-/mayhem}"
cd "$SRC"
PG_CONFIG="$PG_PREFIX/bin/pg_config"
[ -x "$PG_CONFIG" ] || { echo "missing $PG_CONFIG (built by mayhem/Dockerfile)" >&2; exit 1; }
MK=(make -C "$SRC" PG_CONFIG="$PG_CONFIG" CC="$CC")
INSTALL_TEST="$SRC/mayhem/install-test"
OUT="$SRC/mayhem/build-fuzz"

# 1) Test/oracle build: the project's normal flags (pg_config's CFLAGS), no sanitizers.
"${MK[@]}" clean > /dev/null
"${MK[@]}" -j"$MAYHEM_JOBS" COPT="$COVERAGE_FLAGS" > "$SRC/mayhem/make-test.log" 2>&1 \
  || { tail -40 "$SRC/mayhem/make-test.log"; exit 1; }
rm -rf "$INSTALL_TEST"
"${MK[@]}" install DESTDIR="$INSTALL_TEST" > /dev/null

# 2) Fuzz build: every age object sanitized, with SanCov (fuzzer-no-link) so age reports edges.
#    -fno-sanitize=function: PostgreSQL calls through generically typed function pointers (fmgr) by
#    design; with halting UBSan the first such call would abort every input.
#    -fsanitize-address-use-after-return=never: ASan fake stacks move frames to the heap and break
#    PostgreSQL's check_stack_depth().
#    -fno-sanitize=nonnull-attribute: agtype's string concatenation (agtype_ops.c concat_to_agtype_string)
#    calls strncpy(buf, NULL, 0) for every non-string operand of `+`, so this one check would halt on
#    most agtype_add inputs; a zero-length copy reads nothing. All other UBSan checks stay on and halting.
FUZZ_CFLAGS="-O1 $SANITIZER_FLAGS -fno-sanitize=function -fno-sanitize=nonnull-attribute -fsanitize-address-use-after-return=never -fsanitize=fuzzer-no-link $DEBUG_FLAGS"
"${MK[@]}" clean > /dev/null
"${MK[@]}" -j"$MAYHEM_JOBS" COPT="$FUZZ_CFLAGS" > "$SRC/mayhem/make-fuzz.log" 2>&1 \
  || { tail -40 "$SRC/mayhem/make-fuzz.log"; exit 1; }
mkdir -p "$OUT"
# agtype.c/graphid.c cache the OIDs of age's types in static variables that the first use fills with a
# syscache lookup. The in-process harnesses have no catalog, so they link copies of those two objects
# with the variables made global (symbol visibility only, no code change) and preset them, as a
# backend with the extension installed would have them (age_fuzz_args.h).
objcopy --globalize-symbol=g_AGTYPEOID --globalize-symbol=g_AGTYPEARRAYOID \
        --globalize-symbol=g_VERTEXOID --globalize-symbol=g_EDGEOID \
        src/backend/utils/adt/agtype.o "$OUT/agtype_oids.o"
objcopy --globalize-symbol=g_GRAPHIDOID --globalize-symbol=g_GRAPHIDARRAYOID \
        src/backend/utils/adt/graphid.o "$OUT/graphid_oids.o"
AGE_OBJS=$(find src/backend -name '*.o' | sort \
  | sed -e "s#^src/backend/utils/adt/agtype\.o\$#$OUT/agtype_oids.o#" \
        -e "s#^src/backend/utils/adt/graphid\.o\$#$OUT/graphid_oids.o#")

INCS="-I$($PG_CONFIG --includedir-server) -I$SRC/src/include -I$SRC/mayhem"
PGLIBS="-Wl,--start-group $PG_PREFIX/lib/libpgbackend_fuzz.a $PG_PREFIX/lib/libpgcommon_srv.a $PG_PREFIX/lib/libpgport_srv.a -Wl,--end-group -lm -ldl -lpthread"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OUT/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"

for h in fuzz_agtype_in fuzz_cypher_parser fuzz_agtype_ops fuzz_agtype_funcs; do
  $CC $FUZZ_CFLAGS $INCS -c "$SRC/mayhem/$h.c" -o "$OUT/$h.o"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/$h.o" $AGE_OBJS "$OUT/lsan_off.o" $PGLIBS -o "$SRC/$h"
  $CC $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/standalone_main.o" "$OUT/$h.o" $AGE_OBJS "$OUT/lsan_off.o" $PGLIBS -o "$SRC/$h-standalone"
done
