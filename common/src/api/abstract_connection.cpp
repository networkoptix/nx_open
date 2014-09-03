#include "abstract_connection.h"

#include <cstring> /* For std::strstr. */

#include <api/network_proxy_factory.h>
#include <core/resource/resource.h>
#include <utils/serialization/lexical_enum.h>

#include "session_manager.h"
#include "abstract_reply_processor.h"

Q_GLOBAL_STATIC(QnEnumLexicalSerializer<int>, qn_abstractConnection_emptySerializer);

void QnAbstractReplyProcessor::processReply(const QnHTTPRawResponse &response, int handle) {
    Q_UNUSED(response);
    Q_UNUSED(handle);
}

bool QnAbstractReplyProcessor::connect(const char *signal, QObject *receiver, const char *method, Qt::ConnectionType type) {
    if(method && std::strstr(method, "QVariant")) {
        return connect(this, SIGNAL(finished(int, const QVariant &, int)), receiver, method, type);
    } else {
        return connect(this, signal, receiver, method, type);
    }
}

QnAbstractConnection::QnAbstractConnection(QObject *parent, QnResource* targetRes): 
    base_type(parent),
    m_targetRes(targetRes)
{}

QnAbstractConnection::~QnAbstractConnection() {
    return;
}

const QnRequestHeaderList &QnAbstractConnection::extraHeaders() const {
    return m_extraHeaders;
}

void QnAbstractConnection::setExtraHeaders(const QnRequestHeaderList& extraHeaders) {
    m_extraHeaders = extraHeaders;
}

QUrl QnAbstractConnection::url() const {
    return m_url;
}

void QnAbstractConnection::setUrl(const QUrl &url) {
    m_url = url;
}

QnLexicalSerializer *QnAbstractConnection::serializer() const {
    return m_serializer.data() ? m_serializer.data() : qn_abstractConnection_emptySerializer();
}

void QnAbstractConnection::setSerializer(QnLexicalSerializer *serializer) {
    assert(serializer->type() == QMetaType::Int);

    m_serializer.reset(serializer);
}

QString QnAbstractConnection::objectName(int object) const {
    QString result;    
    serializer()->serialize(object, &result);
    return result;
}

int QnAbstractConnection::sendAsyncRequest(int operation, int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, const char *replyTypeName, QObject *target, const char *slot) {
    //TODO #ak all requests are queued in a single thread, so following call is safe: it will not be overwritten by another call before we actually make this call
        //after move to own http implementation (in 2.4) remove it and make proxying transparent by introducing target id to every http request to camera or server
    QnNetworkProxyFactory::instance()->bindHostToResource( m_url.host(), m_targetRes->toSharedPointer() );

    QnAbstractReplyProcessor *processor = newReplyProcessor(object);

    if (target && slot) {
        QByteArray signal;
        if(replyTypeName == NULL) {
            signal = SIGNAL(finished(int, int));
        } else {
            signal = lit("%1finished(int, const %2 &, int)").arg(QSIGNAL_CODE).arg(QLatin1String(replyTypeName)).toLatin1();
        }
        processor->connect(signal.constData(), target, slot, Qt::QueuedConnection);
    }

    QnRequestHeaderList actualHeaders = headers;
    if(!m_extraHeaders.isEmpty())
        actualHeaders.append(m_extraHeaders);

    return QnSessionManager::instance()->sendAsyncRequest(
        operation,
        m_url, 
        objectName(processor->object()), 
        actualHeaders, 
        params, 
        data,
        processor, 
        "processReply"
    );
}

int QnAbstractConnection::sendAsyncGetRequest(int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot) {
    return sendAsyncRequest(QNetworkAccessManager::GetOperation, object, headers, params, QByteArray(), replyTypeName, target, slot);
}

int QnAbstractConnection::sendAsyncGetRequest(int object, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot) {
    return sendAsyncGetRequest(object, QnRequestHeaderList(), params, replyTypeName, target, slot);
}

int QnAbstractConnection::sendAsyncPostRequest(int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, const char *replyTypeName, QObject *target, const char *slot) {
    return sendAsyncRequest(QNetworkAccessManager::PostOperation, object, headers, params, data, replyTypeName, target, slot);
}

int QnAbstractConnection::sendAsyncPostRequest(int object, const QnRequestParamList &params, const QByteArray& data, const char *replyTypeName, QObject *target, const char *slot) {
    return sendAsyncPostRequest(object, QnRequestHeaderList(), params, data, replyTypeName, target, slot);
}

int QnAbstractConnection::sendSyncRequest(int operation, int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QVariant *reply) {
    QnRequestHeaderList actualHeaders = headers;
    if(!m_extraHeaders.isEmpty())
        actualHeaders.append(m_extraHeaders);

    QnHTTPRawResponse response;
    int status = QnSessionManager::instance()->sendSyncRequest(
        operation,
        m_url,
        objectName(object),
        actualHeaders,
        params,
        data,
        response
    );
    if (status != 0)
        return status;

    QScopedPointer<QnAbstractReplyProcessor> processor(newReplyProcessor(object));
    processor->processReply(response, -1);
    if(reply)
        *reply = processor->reply();

    return processor->status();
}

int QnAbstractConnection::sendSyncGetRequest(int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, QVariant *reply) {
    return sendSyncRequest(QNetworkAccessManager::GetOperation, object, headers, params, QByteArray(), reply);
}

int QnAbstractConnection::sendSyncGetRequest(int object, const QnRequestParamList &params, QVariant *reply) {
    return sendSyncGetRequest(object, QnRequestHeaderList(), params, reply);
}

int QnAbstractConnection::sendSyncPostRequest(int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QVariant *reply) {
    return sendSyncRequest(QNetworkAccessManager::PostOperation, object, headers, params, data, reply);
}

int QnAbstractConnection::sendSyncPostRequest(int object, const QnRequestParamList &params, const QByteArray& data, QVariant *reply) {
    return sendSyncPostRequest(object, QnRequestHeaderList(), params, data, reply);
}

