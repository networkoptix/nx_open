#ifndef __REQUEST_HANDLER_H__
#define __REQUEST_HANDLER_H__

#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtCore/QSharedPointer>

#include <nx/utils/string.h>

#include <common/common_globals.h>
#include <utils/common/request_param.h>


class TCPSocket;
class QnRestConnectionProcessor;

// TODO: #MSAPI header-class naming inconsistency. As all derived classes
// are named XyzRestHandler I suggest to rename this one to either
// QnAbstractRestHandler or simply to QnRestHandler. And rename the header.

enum class RestPermissions {
    anyUser,
    adminOnly
};


/**
 * QnRestRequestHandler must be thread-safe.
 *  
 * Single handler instance receives all requests, each request in different thread.
 */
class QnRestRequestHandler: public QObject {
    Q_OBJECT
public:
    QnRestRequestHandler();

    //TODO: #rvasilenko #EC2 replace QnRequestParamList -> QnRequestParams
    //TODO: #rvasilenko replace parameters set with a single struct, 8 arguments is far too many
    //TODO: #rvasilenko looks like QnRestConnectionProcessor* is used only to get and modify its headers
    /*!
        \return http statusCode
    */
    virtual int executeGet(const QString& path, const QnRequestParamList& params,
                           QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) = 0;
    /*!
        \return http statusCode
    */
    virtual int executePost(const QString& path, const QnRequestParamList& params,
                            const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result,
                            QByteArray& resultContentType, const QnRestConnectionProcessor*) = 0;

    virtual void afterExecute(const QString& /*path*/, const QnRequestParamList& /*params*/,
                              const QByteArray& /*body*/, const QnRestConnectionProcessor* /*owner*/) {}

    friend class QnRestProcessorPool;

    Qn::GlobalPermission permissions() const { return m_permissions; }

protected:
    void setPath(const QString &path) { m_path = path; }
    void setPermissions(Qn::GlobalPermission permissions ) {m_permissions = permissions; }
    QString extractAction(const QString &path) const;

protected:
    QString m_path;
    Qn::GlobalPermission m_permissions;
};

typedef QSharedPointer<QnRestRequestHandler> QnRestRequestHandlerPtr;

class QnRestGUIRequestHandlerPrivate;

class QnRestGUIRequestHandler: public QnRestRequestHandler {
    Q_OBJECT
public:
    QnRestGUIRequestHandler();
    virtual ~QnRestGUIRequestHandler();
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, 
                            QByteArray& contentType, const QnRestConnectionProcessor*) override;
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
