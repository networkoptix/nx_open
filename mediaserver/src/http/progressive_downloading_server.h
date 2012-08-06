#ifndef __PROGRESSIVE_DOWNLOADING_SERVER_H__
#define __PROGRESSIVE_DOWNLOADING_SERVER_H__

#include "utils/network/tcp_connection_processor.h"

class QnProgressiveDownloadingProcessor: public QnTCPConnectionProcessor
{
public:
    QnProgressiveDownloadingProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnProgressiveDownloadingProcessor();
protected:
    virtual void run() override;
private:
    QN_DECLARE_PRIVATE_DERIVED(QnProgressiveDownloadingProcessor);
};

#endif // __PROGRESSIVE_DOWNLOADING_SERVER_H__

