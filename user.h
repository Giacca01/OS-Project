#ifndef USER_H
    #define USER_H
    #include "info.h"

    /*
        Definition of a list of transactions
    */
    typedef struct tlist{
        Transaction currTrans;
        struct tlist *nextTrans;
    } TransList;
#endif