#ifndef __UNIVERSAL_REQUEST_PROCESSOR_P_H__
#define __UNIVERSAL_REQUEST_PROCESSOR_P_H__

#include "network/tcp_connection_priv.h"
#include "universal_tcp_listener.h"

class QnUniversalRequestProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnTCPConnectionProcessor* processor;
    QnMutex mutex;
    bool needAuth;

    QnUniversalRequestProcessorPrivate()
    :
        processor( nullptr ),
        needAuth( false )
    {
    }
};

#endif // __UNIVERSAL_REQUEST_PROCESSOR_P_H__
