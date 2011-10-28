#ifndef _REST_CONNECTION_PROCESSOR_H__
#define _REST_CONNECTION_PROCESSOR_H__

#include <QVariantList>
#include "utils/network/tcp_connection_processor.h"

class QnTcpListener;

class QnRestConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnRestConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnRestConnectionProcessor();


protected:
    void run();
private:
    QN_DECLARE_PRIVATE(QnRestConnectionProcessor);
};

#endif // _REST_CONNECTION_PROCESSOR_H__
