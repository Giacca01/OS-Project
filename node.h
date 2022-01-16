#ifndef NODE_H
    #define NODE_H
    #include "info.h"
    /*
        Interval between one dispatch of a transaction
        to a friend and the other, expressed in seconds
    */
    #define TRANS_FRIEND_INTERVAL 2
    
    /* Macro that rappresents the sender with id -1 in Transactions */
    #define NO_SENDER -1
    /*
    #define TEST_ERROR_PARAM                                                       \
    if (errno)                                                                     \
    {                                                                              \
        unsafeErrorPrint("User: failed to read configuration parameter. Error: "); \
        return FALSE;                                                              \
    }*/
#endif