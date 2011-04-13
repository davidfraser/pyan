#include "compiler.h"

#include <stdio.h>


int evaluate_binary_op(EXPRESSION *expr)
{
    EXPRESSION *expr0 = tree_get_child(expr, 0);
    EXPRESSION *expr1 = tree_get_child(expr, 1);
    
    int x0 = CAST_TO_INTEGER(expr0)->value;
    int x1 = CAST_TO_INTEGER(expr1)->value;
    
    switch (tree_type(expr))
    {
        case EXPR_SUM:
            return x0 + x1;
        
        case EXPR_PRODUCT:
            return x0 * x1;
        
        case EXPR_DIFFERENCE:        
            return x0 - x1;
        
        case EXPR_RATIO:        
            return x0 / x1;
        
        case EXPR_LEQ:
            return x0 <= x1;
        
        case EXPR_LT:        
            return x0 < x1;
        
        case EXPR_GEQ:
            return x0 >= x1;
        
        case EXPR_GT:        
            return x0 > x1;
        
        case EXPR_EQ:
            return x0 == x1;
        
        case EXPR_NEQ:        
            return x0 != x1;
        
        case EXPR_AND:
            return x0 && x1;
        
        case EXPR_OR:
            return x0 || x1;
        
        default:
            error("Unhandled %s evaluation!", tree_get_name(expr));
    }
    
    return 0;
}


int is_unary_op(EXPRESSION *expr)
{
    return tree_is_type(expr, EXPR_NEGATION) || tree_is_type(expr, EXPR_NOT);
}


int is_comparison_op(EXPRESSION *expr)
{
    return tree_is_type(expr, EXPR_LEQ) || tree_is_type(expr, EXPR_LT)
            || tree_is_type(expr, EXPR_GEQ) || tree_is_type(expr, EXPR_GT)
            || tree_is_type(expr, EXPR_EQ) || tree_is_type(expr, EXPR_NEQ);
}


int is_binary_op(EXPRESSION *expr)
{
    return tree_is_type(expr, EXPR_SUM) || tree_is_type(expr, EXPR_PRODUCT)
            || tree_is_type(expr, EXPR_DIFFERENCE) || tree_is_type(expr, EXPR_RATIO)
            || tree_is_type(expr, EXPR_LEQ) || tree_is_type(expr, EXPR_LT)
            || tree_is_type(expr, EXPR_GEQ) || tree_is_type(expr, EXPR_GT)
            || tree_is_type(expr, EXPR_EQ) || tree_is_type(expr, EXPR_NEQ)
            || tree_is_type(expr, EXPR_AND) || tree_is_type(expr, EXPR_OR);
}


int is_commutable_op(EXPRESSION *expr)
{
    return tree_is_type(expr, EXPR_SUM) || tree_is_type(expr, EXPR_PRODUCT)
            || tree_is_type(expr, EXPR_EQ) || tree_is_type(expr, EXPR_NEQ)
            || tree_is_type(expr, EXPR_AND) || tree_is_type(expr, EXPR_OR);
}


NODE_TYPE negate_comparison(NODE_TYPE type)
{
    switch (type)
    {
        case EXPR_GT:
            return EXPR_LEQ;
        case EXPR_LT:
            return EXPR_GEQ;
        case EXPR_GEQ:
            return EXPR_LT;
        case EXPR_LEQ:
            return EXPR_GT;
        case EXPR_EQ:
            return EXPR_NEQ;
        case EXPR_NEQ:
            return EXPR_EQ;
        default:
            error("Unknown comparison type %d\n", type);
    }
    
    return NULL_NODE_TYPE;
}


char *get_op_symbol(EXPRESSION *expr)
{
    switch (expr->node.type)
    {
        case EXPR_EQ:
            return "==";
        case EXPR_NEQ:
            return "!=";
        case EXPR_LEQ:
            return "<=";
        case EXPR_GEQ:
            return ">=";
        case EXPR_LT:
            return "<";
        case EXPR_GT:
            return ">";
        case EXPR_AND:
            return "&&";
        case EXPR_OR:
            return "||";
        case EXPR_PRODUCT:
            return "*";
        case EXPR_SUM:
            return "+";
        case EXPR_DIFFERENCE:
            return "-";
        case EXPR_RATIO:
            return "/";
        case EXPR_NEGATION:
            return "-";
        case EXPR_NOT:
            return "!";
        default:
            return "?";
    }
}
