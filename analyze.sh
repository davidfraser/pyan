#!/bin/bash
if [ $# -lt 1 ]; then
  echo -ne "\n"
  echo -ne "USAGE: $0 <pyfiles>\n\n"
  echo -ne "where pyfiles is one or more Python source code files (*.py).\n"
  echo -ne "\n"
  exit 255
fi

echo -ne "Analyzing...\n"
python -m pyan/pyan $@ -c -e --dot >temp.dot
if [ $? -eq 0 ]; then
  echo -ne "Generating layout...\n"
  # "fdp" comes from the graphviz package
#  fdp -Txdot -otemp.xdot temp.dot
  dot -Txdot -otemp.xdot temp.dot

  echo -ne "Visualizing...\n"
  # -n = filter off (input already in xdot format)
  python -m xdot/xdot -n temp.xdot
fi
