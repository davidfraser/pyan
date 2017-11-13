#!/bin/bash
./pyan.py pyan/*.py --no-uses --defines --grouped --nested-groups --colored --dot --annotated >defines.dot
./pyan.py pyan/*.py --uses --no-defines --grouped --nested-groups --colored --dot --annotated >uses.dot
dot -Tsvg defines.dot >defines.svg
dot -Tsvg uses.dot >uses.svg
echo -ne "Pyan architecture: generated defines.svg and uses.svg\n"
