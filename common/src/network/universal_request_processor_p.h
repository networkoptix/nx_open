#ifndef __UNIVERSAL_REQUEST_PROCESSOR_P_H__
#define __UNIVERSAL_REQUEST_PROCESSOR_P_H__

#include "utils/network/tcp_connection_priv.h"

class QnUniversalRequestProcessorPrivate: public QnTCPConnectionProcessorPrivate
{
public:
    QnTCPConnectionProcessor* processor;
    QMutex mutex;
    QnTcpListener* owner;
};

#endif // __UNIVERSAL_REQUEST_PROCESSOR_P_H__
