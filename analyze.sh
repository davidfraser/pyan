#!/bin/bash
if [ $# -lt 1 ]; then
  echo -ne "\n"
  echo -ne "USAGE: $0 <pyfiles>\n\n"
  echo -ne "where pyfiles is one or more Python source code files (*.py).\n"
  echo -ne "\n"
  exit 255
fi

echo -ne "Analyzing...\n"
python -m pyan $@ -c --dot >temp.dot
if [ $? -eq 0 ]; then
  echo -ne "Visualizing...\n"
  python -m xdot -f fdp temp.dot
fi
