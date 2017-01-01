#!/bin/sh

DIR="$(cd "$(dirname "${0})" && pwd)"

python "${DIR}"/genrandom.py 100 \
    | tee "${DIR}"/../tests/integration/fixtures/random.txt \
    | cdbmake "${DIR}"/../tests/integration/fixtures/random.cdb{,.tmp}
