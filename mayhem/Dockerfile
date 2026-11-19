# syntax=docker/dockerfile:1.7
#
# Commit-image Dockerfile (SMALL form, PREFERRED) — builds ON TOP of the org C/C++ base
# image (ghcr.io/savantenvs/base). The base already provides debian + clang/llvm +
# libFuzzer/afl++ + the identities + the build-contract ENV, so this only adds the
# per-commit build (built on top of the prebuilt base, rather than building the toolchain in-image).
#
# Build (from the repo root):
#   DOCKER_BUILDKIT=1 docker build -f mayhem/Dockerfile \
#       --build-arg COMMIT=$(git rev-parse HEAD) \
#       [--build-arg SOURCE_DATE_EPOCH=$(date +%s)] \
#       -t mytag .

# The org base, pinned to a fixed DATE-tagged base (YYYYMMDD; shared by all integrations
# minted against it). Use the current dated tag; pin by digest in production: ...base@sha256:<digest>.
FROM ghcr.io/savantenvs/base:20261023

ARG COMMIT
ARG REPO_URL
# SOURCE_DATE_EPOCH stays an ARG (available to build.sh); do NOT bake it as an ENV — an
# empty value breaks clang ("must be a non-negative decimal integer") both here and for
# any in-image rebuild. build.sh unsets it when empty.
ARG SOURCE_DATE_EPOCH

# Sanitizer override: the base exports SANITIZER_FLAGS (ASan+UBSan, halting) as the default.
# To change it for a build (e.g. ASan-only, or add MSan): --build-arg SANITIZER_FLAGS="...".
# Declare it WITHOUT a default (no '='): an undefaulted ARG lets the base ENV flow through to
# build.sh when not passed, and an explicit --build-arg overrides it. (An empty default — `=` —
# would instead shadow the base ENV and silently wipe the sanitizers.)
ARG SANITIZER_FLAGS
# Coverage (optional, off by default): instrument the TEST build to measure how much of the project
# the suite exercises — --build-arg COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping".
# Empty by default (no effect on the normal oracle build); build.sh appends it to the test build only.
ARG COVERAGE_FLAGS=
# Build parallelism (optional). build.sh falls back to nproc when unset: --build-arg MAYHEM_JOBS=8
ARG MAYHEM_JOBS=

# If mayhem/build.sh needs EXTRA apt packages, install them as root before the build
# (re-pin to SOURCE_DATE_EPOCH first for reproducibility — repro-sources-list.sh ships
# in the base):
#   USER root
#   RUN [ -n "$SOURCE_DATE_EPOCH" ] && repro-sources-list.sh || true ; \
#       apt-get update && apt-get install -yq --no-install-recommends <pkgs> \
#    && rm -rf /var/lib/apt/lists/*
#   USER mayhem

# Anything the upstream build would download, or a library apt lacks or ships at the wrong version, is
# built once from pinned source into a static prefix under /opt/toolchains/deps/<name>-<version>/
# (root-owned, world-readable, no .so); build.sh just points at it, e.g. via PKG_CONFIG_PATH
# (SPEC §6.2 item 19). The source stays in $d/src and the build runs out of tree with -gdwarf-3, so
# backtraces show /opt/toolchains/deps/... paths and the harness binary stays DWARF <= 3. Example:
#   USER root
#   ARG JANSSON_VER=2.14.1
#   ARG JANSSON_SHA256=<sha256 of the tarball>
#   RUN d=/opt/toolchains/deps/jansson-${JANSSON_VER} && mkdir -p "$d/src" /tmp/dep-build \
#    && curl -fsSL -o /tmp/dep.tgz \
#         https://github.com/akheron/jansson/releases/download/v${JANSSON_VER}/jansson-${JANSSON_VER}.tar.gz \
#    && echo "${JANSSON_SHA256}  /tmp/dep.tgz" | sha256sum -c - \
#    && tar -xzf /tmp/dep.tgz --strip-components=1 -C "$d/src" && cd /tmp/dep-build \
#    && "$d/src/configure" --prefix="$d" --disable-shared --enable-static CFLAGS="-O2 -g -gdwarf-3" \
#    && make -j"$(nproc)" install \
#    && cd / && rm -rf /tmp/dep-build /tmp/dep.tgz && chmod -R a+rX "$d"
#   USER mayhem

# PostgreSQL 18 (the server age is an extension of) is not in the repo and apt has no sanitizer-ready
# backend objects, so a pinned release is built once here (normal flags, uninstrumented, DWARF 3) into
# /opt/toolchains/deps/postgresql-<ver>/: an installed server (bin/, include/server, lib/pgxs) that
# mayhem/test.sh runs age's regression suite against, plus lib/libpgbackend_fuzz.a, every backend object
# (main.o with main() renamed, pg_locale.o with its static default_locale made global) that
# mayhem/build.sh links into the in-process harnesses together with the sanitized age objects.
USER root
ARG PG_VER=18.7
ARG PG_SHA256=441dfae4280cae3e0425e5f876f80559fb0731c140f1aae22b3e5a54d0411bfd
RUN d=/opt/toolchains/deps/postgresql-${PG_VER} && mkdir -p "$d/src" /tmp/pg-build \
 && curl -fsSL -o /tmp/pg.tar.bz2 https://ftp.postgresql.org/pub/source/v${PG_VER}/postgresql-${PG_VER}.tar.bz2 \
 && echo "${PG_SHA256}  /tmp/pg.tar.bz2" | sha256sum -c - \
 && tar -xjf /tmp/pg.tar.bz2 --strip-components=1 -C "$d/src" && cd /tmp/pg-build \
 && CC=clang CFLAGS="-O2 -g -gdwarf-3" "$d/src/configure" --prefix="$d" --without-readline --without-icu \
      --without-openssl --without-zlib --without-zstd --without-lz4 --without-libxml --without-libcurl \
      --without-liburing > /tmp/pg-configure.log 2>&1 \
 && make -j"$(nproc)" > /tmp/pg-make.log 2>&1 && make -C contrib/fuzzystrmatch > /dev/null \
 && make install > /dev/null && make -C contrib/fuzzystrmatch install > /dev/null \
 && objcopy --redefine-sym main=pg_server_main src/backend/main/main.o /tmp/pg-build/pg_main_renamed.o \
 && objcopy --globalize-symbol=default_locale src/backend/utils/adt/pg_locale.o /tmp/pg-build/pg_locale_exported.o \
 && cat $(find src/backend -mindepth 2 -maxdepth 2 -name objfiles.txt | sort) src/timezone/objfiles.txt \
      | tr ' ' '\n' | grep -v '^$' \
      | sed -e 's#^src/backend/main/main\.o$#pg_main_renamed.o#' \
            -e 's#^src/backend/utils/adt/pg_locale\.o$#pg_locale_exported.o#' > /tmp/pg-objs.txt \
 && ar qcs "$d/lib/libpgbackend_fuzz.a" $(cat /tmp/pg-objs.txt) \
 && cp src/common/libpgcommon_srv.a src/port/libpgport_srv.a "$d/lib/" \
 && cd / && rm -rf /tmp/pg-build /tmp/pg.tar.bz2 /tmp/pg-*.log /tmp/pg-objs.txt && chmod -R a+rX,go-w "$d"
ENV PG_PREFIX=/opt/toolchains/deps/postgresql-18.7

# Source enters from the build CONTEXT (the CI checkout): the FULL repo (incl. .git
# history) is copied to /mayhem as `mayhem`; build.sh builds in place.
USER mayhem
WORKDIR /mayhem
COPY --chown=2000:2000 . /mayhem

# Anything a Dockerfile step materializes into the tree (downloaded fixtures, generated headers,
# vendored deps) MUST be committed or (re)created by build.sh: rlenv's patch grader runs
# `git clean -ffdX` before every graded build, so a gitignored, untracked file is gone by then and
# every honest patch scores zero (SPEC §6.2 item 16; verify-repo.sh checks it).
#
# OPTIONAL compile-cache pre-fill (SPEC §6.2 item 18). Bases minted with /opt/mayhem/ccache/prefill
# run the build under rlenv's pinned ccache, so the patch grader's first (otherwise cold) rebuild is
# warm — this is what keeps large C/C++ targets inside the build time limit. Older bases fall back to
# the plain build; a missing or stale pre-fill only costs a cold cache. Never enable ccache in
# build.sh or via ENV/PATH, and keep the compiler resolving by name (CC=clang) or nothing is cached.
RUN if [ -x /opt/mayhem/ccache/prefill ]; then /opt/mayhem/ccache/prefill bash mayhem/build.sh; \
    else bash mayhem/build.sh; fi

# Run the project's functional tests at build time IF the repo ships them (mayhem/test.sh is
# optional — omit it only when the repo has no tests meaningful for RL). A failing suite fails the
# image build, so the commit image is guaranteed to build AND pass its own tests.
RUN if [ -f mayhem/test.sh ]; then bash mayhem/test.sh; else echo "no mayhem/test.sh — repo ships no functional tests"; fi

LABEL org.opencontainers.image.source="${REPO_URL}" \
      org.opencontainers.image.revision="${COMMIT}" \
      rlenv.tier="commit" \
      rlenv.source_date_epoch="${SOURCE_DATE_EPOCH}" \
      rlenv.user="mayhem"

USER mayhem
WORKDIR /mayhem
