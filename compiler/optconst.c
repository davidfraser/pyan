/*
 * optconst.c - Implementation of constant-folding optimisation.
 *
 * Copyright (C) 2003, Edmund Horner.
 */

#include "compiler.h"

/*
 * ALGORITHM
 *
 * For each expression in the program with all-constant subexpressions,
 * evaluate and replace with the constant result.
 */
int optimise_constant_folding(MODULE *module)
{
    return 0;
};
