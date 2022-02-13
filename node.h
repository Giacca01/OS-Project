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
    #define NO_SEND_TRANSACTION_ATTEMPS 10
#endif