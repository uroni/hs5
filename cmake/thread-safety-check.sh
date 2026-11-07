#!/bin/bash
# Runs clang's thread safety analysis (-Wthread-safety) over hs5 sources.
# Only libc++ annotates std::mutex/std::scoped_lock (std::unique_lock is not
# annotated, use CondVarLock from src/ThreadSafety.h instead), so this
# compiles with -stdlib=libc++ in syntax-only mode, reusing the include paths
# and defines of an existing (libstdc++) build. Nothing is linked, so the
# dependencies do not have to be rebuilt with libc++.
#
# Usage: cmake/thread-safety-check.sh [build-dir] [source files...]
# Defaults to builds/ninja-multi-vcpkg-lld and the annotated sources.
# Requires clang and libc++ (apt install clang libc++-dev libc++abi-dev).

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-$ROOT/builds/ninja-multi-vcpkg-lld}"
shift || true
CLANGXX="${CLANGXX:-clang++}"

if [[ ! -f "$BUILD_DIR/compile_commands.json" ]]; then
  echo "thread-safety-check: No compile_commands.json in '$BUILD_DIR'" >&2
  exit 1
fi

if [[ $# -gt 0 ]]; then
  FILES=("$@")
else
  # Sources that are annotated: the .cpp or its .h includes ThreadSafety.h
  # (other files use std::mutex in ways the analysis cannot follow, e.g. SCOPE_EXIT unlocks)
  FILES=()
  for cpp in "$ROOT"/src/*.cpp; do
    if grep -qs '#include "ThreadSafety.h"' "$cpp" "${cpp%.cpp}.h"; then
      FILES+=("$cpp")
    fi
  done
fi

PRELUDE="$(mktemp --suffix=.h)"
trap 'rm -f "$PRELUDE"' EXIT
# folly was configured against libstdc++, which provides <ext/random>
cat > "$PRELUDE" <<'EOF'
#include <folly/folly-config.h>
#undef FOLLY_HAVE_EXTRANDOM_SFMT19937
EOF

FLAGS=(-stdlib=libc++ -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS -fexperimental-library
  -include "$PRELUDE" -fsyntax-only -Wthread-safety -Werror=thread-safety)

status=0
for file in "${FILES[@]}"; do
  file="$(realpath "$file")"
  # Compile arguments of the first configuration for this file, minus output/dependency options
  mapfile -d '' -t ARGS < <(python3 - "$BUILD_DIR/compile_commands.json" "$file" <<'EOF'
import json, shlex, sys
for e in json.load(open(sys.argv[1])):
    if e["file"] != sys.argv[2]:
        continue
    args = shlex.split(e["command"])[1:]
    out = [e["directory"]]
    skip = False
    for a in args:
        if skip:
            skip = False
        elif a in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
        elif a in ("-c", "-MD", "-MMD") or a.startswith("@") or a.startswith("-fmodule") or a.startswith("-fdeps"):
            pass
        else:
            out.append(a)
    sys.stdout.write("\0".join(out) + "\0")
    break
EOF
  )
  if [[ ${#ARGS[@]} -eq 0 ]]; then
    echo "thread-safety-check: '$file' not found in compile_commands.json" >&2
    status=1
    continue
  fi
  echo "thread-safety-check: $file"
  (cd "${ARGS[0]}" && "$CLANGXX" "${FLAGS[@]}" "${ARGS[@]:1}") || status=1
done

# Lambdas passed to S3Handler::runInHttpThread must be annotated with REQUIRES(<self>->httpThread).
# The analysis only flags an unannotated lambda if it accesses guarded members, and attributes are
# not part of the lambda type, so this cannot be checked in the template itself.
# S3Handler and ApiHandler must not use runInEventBaseThread directly, as that bypasses the check, and must use
# downstream() instead of downstream_, which checks that the request is still active.
python3 - "$ROOT"/src/*.cpp "$ROOT"/src/*.h <<'EOF' || status=1
import os, re, sys
ok = True
for fn in sys.argv[1:]:
    src = open(fn).read()
    if os.path.basename(fn).startswith(("s3handler.", "ApiHandler.")):
        for m in re.finditer(r"\brunInEventBaseThread\(", src):
            line_start = src.rfind("\n", 0, m.start()) + 1
            if "//" in src[line_start:m.start()]:
                continue  # Comment
            if "void runInHttpThread(" in src[max(0, m.start() - 200):m.start()]:
                continue  # Used by runInHttpThread itself
            line = src.count("\n", 0, m.start()) + 1
            print(f"{fn}:{line}: error: use runInHttpThread instead of runInEventBaseThread", file=sys.stderr)
            ok = False
        if os.path.basename(fn).startswith("s3handler."):
            for m in re.finditer(r"getGlobalCPUExecutor\(\)->add\(", src):
                line_start = src.rfind("\n", 0, m.start()) + 1
                if "//" in src[line_start:m.start()]:
                    continue  # Comment
                if re.search(r"void runInCpuThread(FireAndForget)?\(", src[max(0, m.start() - 200):m.start()]):
                    continue  # Used by runInCpuThread/runInCpuThreadFireAndForget itself
                line = src.count("\n", 0, m.start()) + 1
                print(f"{fn}:{line}: error: use runInCpuThread or runInCpuThreadFireAndForget instead of getGlobalCPUExecutor()->add in S3Handler", file=sys.stderr)
                ok = False
        for m in re.finditer(r"\bdownstream_\b", src):
            line_start = src.rfind("\n", 0, m.start()) + 1
            if "//" in src[line_start:m.start()]:
                continue  # Comment
            if "ResponseHandler* downstream() const" in src[max(0, m.start() - 200):m.start()]:
                continue  # Used by downstream() itself
            line = src.count("\n", 0, m.start()) + 1
            print(f"{fn}:{line}: error: use downstream() instead of downstream_", file=sys.stderr)
            ok = False
    for m in re.finditer(r"\brunInHttpThread\(", src):
        head = src[m.end():src.find("{", m.end())]
        if "F&&" in head:
            continue  # Definition
        if not re.search(r"\)\s*REQUIRES\(\s*\w+(->|\.)httpThread\s*\)", head):
            line = src.count("\n", 0, m.start()) + 1
            print(f"{fn}:{line}: error: lambda passed to runInHttpThread must be annotated with REQUIRES(self->httpThread)", file=sys.stderr)
            ok = False
sys.exit(0 if ok else 1)
EOF

exit $status
