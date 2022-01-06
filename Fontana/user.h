#ifndef USER_H
    #define USER_H
    #include "info.h"
/*
    Macro that tests whether strtol or getenv
    returned and error
*/
    #define TEST_ERROR_PARAM \
        if (errno)           \
        {                      \
            unsafeErrorPrint("User: failed to read configuration parameter. Error: ");\
            return FALSE;\
        }
    
    #define TEST_MALLOC_ERROR(ptr)\
        if (ptr == NULL)\
            unsafeErrorPrint("User: failed to allocate memory. Error: ");\
            return FALSE;
    typedef struct tlist{
        Transaction currTrans; /* Stefano: ho rimosso il puntatore perché dava problemi nell'inserimento */
        struct tlist *nextTrans;
    } TransList;
#endif