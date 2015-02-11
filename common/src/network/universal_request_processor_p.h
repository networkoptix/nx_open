#ifndef __UNIVERSAL_REQUEST_PROCESSOR_P_H__
#define __UNIVERSAL_REQUEST_PROCESSOR_P_H__

#include "utils/network/tcp_connection_priv.h"

class QnUniversalRequestProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnTCPConnectionProcessor* processor;
    QnMutex mutex;
    QnTcpListener* owner;
    bool needAuth;

    QnUniversalRequestProcessorPrivate()
    :
        processor( nullptr ),
        owner( nullptr ),
        needAuth( false )
    {
    }
};

#endif // __UNIVERSAL_REQUEST_PROCESSOR_P_H__
