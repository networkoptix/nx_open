#ifndef __REQUEST_HANDLER_H__
#define __REQUEST_HANDLER_H__

#include <QObject>
#include <QPair>
#include <QString>
#include <QList>
#include <QByteArray>
#include "utils/common/request_param.h"
#include "utils/common/pimpl.h"


class TCPSocket;
class QnRestRequestHandler: public QObject
{
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result) = 0;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result) = 0;

    // incoming connection socket
    virtual QString description(TCPSocket* tcpSocket) const { Q_UNUSED(tcpSocket) return QString(); }
    friend class QnRestServer;
protected:
    void setPath(const QString& path) { m_path = path; }
protected:
    QString m_path;
};

class QnRestGUIRequestHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    QnRestGUIRequestHandler();
    virtual ~QnRestGUIRequestHandler();
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result);
protected:
    virtual int executeGetGUI(const QString& path, const QnRequestParamList& params, QByteArray& result) = 0;
    virtual int executePostGUI(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result) = 0;
private:
    Q_INVOKABLE void methodExecutor();
protected:
    QN_DECLARE_PRIVATE(QnRestGUIRequestHandler);
};

#endif // __REQUEST_HANDLER_H__
