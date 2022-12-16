#ifndef DTYPE_H
#define DTYPE_H

#include <stdlib.h>

#define ULL unsigned long long

typedef enum {
    DTYPE_BYTE,
    DTYPE_SHORT,
    DTYPE_INT,
    DTYPE_LONG,
    DTYPE_BOOL,
    DTYPE_STRING,
    DTYPE_FLOAT,
    DTYPE_DOUBLE,
    DTYPE_LOCAL_ARRAY,
    DTYPE_LOCAL_STRUCT,
    DTYPE_FUNC,
    DTYPE_ARRAY,
    DTYPE_STRUCT,

    DTYPE_MAX
}DType;
//List<Stu<A>>

//typedef struct object object;
//struct object{
//    volatile int ref;
//    void* data;
//};

//ULL h7_data_type_make(int base, int* subs, int len_sub){}
//int h7_data_type_get_base(ULL val);
//int* h7_data_type_get_subs(ULL val, int* len_out);


#endif // DTYPE_H
