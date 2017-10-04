#ifndef QN_CONNECTION_H
#define QN_CONNECTION_H

#include <cassert>

#ifndef Q_MOC_RUN
#include <boost/preprocessor/stringize.hpp>
#endif

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QScopedPointer>
#include <QtCore/QEventLoop>

#include <utils/common/request_param.h>
#include <utils/common/warnings.h>
#include <utils/common/connective.h>

#include <nx_ec/ec_api.h>
#include <common/common_module_aware.h>

class QnLexicalSerializer;
class QnAbstractReplyProcessor;

namespace QnStringizeTypeDetail { template<class T> void check_type() {} }

/**
 * Macro that stringizes the given type name and checks at compile time that
 * the given type is actually defined.
 *
 * \param TYPE                          Type to stringize.
 */
#define QN_STRINGIZE_TYPE(TYPE) (QnStringizeTypeDetail::check_type<TYPE>(), BOOST_PP_STRINGIZE(TYPE))


class QnConnectionRequestResult: public QObject {
    Q_OBJECT
public:
    QnConnectionRequestResult(QObject *parent = NULL): QObject(parent), m_finished(false), m_status(0), m_handle(0) {}

    bool isFinished() const { return m_finished; }
    int status() const { return m_status; }
    int handle() const { return m_handle; }
    const QVariant &reply() const { return m_reply; }
    template<class T>
    T reply() const { return m_reply.value<T>(); }

    /**
     * Starts an event loop waiting for the reply.
     *
     * \returns                         Received request status.
     */
    int exec() {
        QEventLoop loop;
        connect(this, SIGNAL(replyProcessed()), &loop, SLOT(quit()));
        loop.exec();

        return m_status;
    }

signals:
    void replyProcessed();

public slots:
    void processReply(int status, const QVariant &reply, int handle) {
        m_finished = true;
        m_status = status;
        m_reply = reply;
        m_handle = handle;

        emit replyProcessed();
    }

protected:
    bool m_finished;
    int m_status;
    int m_handle;
    QVariant m_reply;
};


class QnEc2ConnectionRequestResult: public QnConnectionRequestResult {
    Q_OBJECT
public:
    QnEc2ConnectionRequestResult(QObject *parent = NULL):
        QnConnectionRequestResult(parent)
    {}

    ec2::AbstractECConnectionPtr connection() const { return m_connection; }

public slots:
    void processEc2Reply( int handle, ec2::ErrorCode errorCode, ec2::AbstractECConnectionPtr connection )
    {
        m_connection = connection;
        QnConnectionInfo reply;
        if (connection)
            reply = connection->connectionInfo();
        processReply(static_cast<int>(errorCode), QVariant::fromValue(reply), handle);
    }

private:
    ec2::AbstractECConnectionPtr m_connection;
};



class QnAbstractConnection: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnAbstractConnection(
        QnCommonModule* commonModule,
        const QnResourcePtr& targetRes = QnResourcePtr());
    virtual ~QnAbstractConnection();

    nx::utils::Url url() const;
    void setUrl(const nx::utils::Url &url);

protected:
    virtual QnAbstractReplyProcessor *newReplyProcessor(int object, const QString& serverId) = 0;

    QnLexicalSerializer *serializer() const;
    void setSerializer(QnLexicalSerializer *serializer);

    QString objectName(int object) const;

    const nx_http::HttpHeaders& extraHeaders() const;
    void setExtraHeaders(nx_http::HttpHeaders extraHeaders);
    const QnRequestParamList &extraQueryParameters() const;
    void setExtraQueryParameters(const QnRequestParamList &extraQueryParameters);

    int sendAsyncRequest(nx_http::Method::ValueType method, int object, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, const char *replyTypeName, QObject *target, const char *slot);
    int sendAsyncGetRequest(int object, nx_http::HttpHeaders headers, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot);
    int sendAsyncGetRequest(int object, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot);
    int sendAsyncPostRequest(int object, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, const char *replyTypeName, QObject *target, const char *slot);
    int sendAsyncPostRequest(int object, const QnRequestParamList &params, QByteArray msgBody, const char *replyTypeName, QObject *target, const char *slot);

    int sendSyncRequest(nx_http::Method::ValueType method, int object, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, QVariant *reply);
    int sendSyncGetRequest(int object, nx_http::HttpHeaders headers, const QnRequestParamList &params, QVariant *reply);
    int sendSyncGetRequest(int object, const QnRequestParamList &params, QVariant *reply);
    int sendSyncPostRequest(int object, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, QVariant *reply);
    int sendSyncPostRequest(int object, const QnRequestParamList &params, QByteArray msgBody, QVariant *reply);

    template<class T>
    int sendSyncRequest(nx_http::Method::ValueType method, int object, nx_http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody, T *reply) {
        NX_ASSERT(reply);

        QVariant replyVariant;
        int status = sendSyncRequest(method, object, std::move(headers), params, std::move(msgBody), &replyVariant);

        if (status)
            return status;

        int replyType = qMetaTypeId<T>();
        if(replyVariant.userType() != replyType)
            qnWarning("Invalid return type of request '%1': expected '%2', got '%3'.", objectName(object), QMetaType::typeName(replyType), replyVariant.typeName());

        *reply = replyVariant.value<T>();
        return status;
    }

    template<class T>
    int sendSyncGetRequest(int object, nx_http::HttpHeaders headers, const QnRequestParamList &params, T *reply) {
        return sendSyncRequest(nx_http::Method::get, object, headers, params, QByteArray(), reply);
    }

    template<class T>
    int sendSyncGetRequest(int object, const QnRequestParamList &params, T *reply) {
        return sendSyncGetRequest(object, QnRequestHeaderList(), params, reply);
    }

    QnResourcePtr targetResource() const;
    virtual bool isReady() const;

private:
    static bool connectProcessor(QnAbstractReplyProcessor *sender, const char *signal, QObject *receiver, const char *method, Qt::ConnectionType connectionType = Qt::AutoConnection);

private:
    nx::utils::Url m_url;
    QScopedPointer<QnLexicalSerializer> m_serializer;
    nx_http::HttpHeaders m_extraHeaders;
    QnRequestParamList m_extraQueryParameters;
    QnResourcePtr m_targetRes;
};


#endif // QN_CONNECTION_H
