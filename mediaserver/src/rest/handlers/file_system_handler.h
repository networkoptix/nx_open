#ifndef QN_FILE_SYSTEM_HANDLER_H
#define QN_FILE_SYSTEM_HANDLER_H

#include "rest/server/request_handler.h"

class QnGlobalMonitor;

class QnFileSystemHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    QnFileSystemHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
private:


    QnGlobalMonitor *m_monitor;
};

#endif // QN_FILE_SYSTEM_HANDLER_H
