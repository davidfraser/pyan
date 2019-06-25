#!/bin/bash
VERSION="$1"
twine upload dist/pyan3-${VERSION}.tar.gz dist/pyan3-${VERSION}-py3-none-any.whl
