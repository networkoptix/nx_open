#ifndef __REQUEST_HANDLER_H__
#define __REQUEST_HANDLER_H__

#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtCore/QSharedPointer>

#include "utils/common/request_param.h"

class TCPSocket;

// TODO: #MSAPI header-class naming inconsistency. As all derived classes
// are named XyzRestHandler I suggest to rename this one to either
// QnAbstractRestHandler or simply to QnRestHandler. And rename the header.


/**
 * QnRestRequestHandler must be thread-safe.
 *  
 * Single handler instance receives all requests, each request in different thread.
 */
class QnRestRequestHandler: public QObject {
    Q_OBJECT
public:

    // TODO: #Elric #EC2 replace QnRequestParamList -> QnRequestParams

    /*!
        \return http statusCode
    */
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType) = 0;
    /*!
        \return http statusCode
    */
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& resultContentType) = 0;

    
    friend class QnRestProcessorPool;

protected:
    void setPath(const QString &path) { m_path = path; }
    
    qint64 parseDateTime(const QString &dateTime) const;
    QString extractAction(const QString &path) const;

protected:
    QString m_path;
};

typedef QSharedPointer<QnRestRequestHandler> QnRestRequestHandlerPtr;

class QnRestGUIRequestHandlerPrivate;

class QnRestGUIRequestHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    QnRestGUIRequestHandler();
    virtual ~QnRestGUIRequestHandler();
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& contentType);
protected:
    virtual int executeGetGUI(const QString& path, const QnRequestParamList& params, QByteArray& result) = 0;
    virtual int executePostGUI(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result) = 0;
private:
    Q_INVOKABLE void methodExecutor();
protected:
    Q_DECLARE_PRIVATE(QnRestGUIRequestHandler);

    QnRestGUIRequestHandlerPrivate *d_ptr;
};

#endif // __REQUEST_HANDLER_H__
