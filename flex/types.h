#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h> /* Needed for free/malloc declarations if used in inline functions */

/* Definition for Math Expressions */
typedef struct {
    int type;      // 0 = int, 1 = float, 2 = string
    int i_val;
    float f_val;
    char *s_val;   
} ExprVal;

/* Definition for Print Items */
typedef struct print_item {
    enum { PRINT_STRING, PRINT_CHAR, PRINT_EXPR } type; 
    union {
        char *str;
        char char_val;
        struct {
            int type; // 0=int, 1=float, 2=string
            union {
                int i_val;
                float f_val;
                char *s_val;
            } value;
        } expr_val;
    } value;
    struct print_item *next;
} print_item;

#endif