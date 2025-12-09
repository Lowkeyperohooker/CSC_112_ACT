#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h> 

/* Definition for Math Expressions */
typedef struct {
    int type;      // 0 = int, 1 = float, 2 = string, 3 = char
    int i_val;     // used for int and char (ASCII)
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
            int type; // 0=int, 1=float, 2=string, 3=char
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