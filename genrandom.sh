#!/bin/sh

python ./genrandom.py 100 | tee tests/integration/fixtures/random.txt \
    | cdbmake tests/integration/fixtures/random.cdb{,.tmp}
