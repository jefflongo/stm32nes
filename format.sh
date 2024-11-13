#!/bin/bash
set -e

find . \( -name *.c -o -name *.h \) -not -path "*/build/*" -exec clang-format -style=file -i {} \;
find . \( -name CMakeLists.txt -o -name *.cmake \) -not -path "*/build/*" -exec cmake-format -i {} \;
