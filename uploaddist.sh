#!/bin/bash
VERSION="$1"
twine upload dist/pyan-${VERSION}.tar.gz dist/pyan-${VERSION}-py3-none-any.whl
