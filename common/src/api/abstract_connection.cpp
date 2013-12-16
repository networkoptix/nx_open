#include "abstract_connection.h"

#include <cstring> /* For std::strstr. */

#include <utils/common/enum_name_mapper.h>

#include "session_manager.h"

Q_GLOBAL_STATIC(QnEnumNameMapper, qn_abstractConnection_emptyNameMapper);

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

QnAbstractConnection::QnAbstractConnection(QObject *parent): 
    base_type(parent)
{}

QnAbstractConnection::~QnAbstractConnection() {
    return;
}

QUrl QnAbstractConnection::url() const {
    return m_url;
}

void QnAbstractConnection::setUrl(const QUrl &url) {
    m_url = url;
}

QnEnumNameMapper *QnAbstractConnection::nameMapper() const {
    return m_nameMapper.data() ? m_nameMapper.data() : qn_abstractConnection_emptyNameMapper();
}

void QnAbstractConnection::setNameMapper(QnEnumNameMapper *nameMapper) {
    m_nameMapper.reset(nameMapper);
}

int QnAbstractConnection::sendAsyncRequest(int operation, int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, const char *replyTypeName, QObject *target, const char *slot) {
    QnAbstractReplyProcessor *processor = newReplyProcessor(object);

    if (target && slot) {
        QByteArray signal;
        if(replyTypeName == NULL) {
            signal = SIGNAL(finished(int, int));
        } else {
            signal = QString::fromLatin1("%1finished(int, const %2 &, int)").arg(QSIGNAL_CODE).arg(QLatin1String(replyTypeName)).toLatin1();
        }
        processor->connect(signal.constData(), target, slot, Qt::QueuedConnection);
    }

    return QnSessionManager::instance()->sendAsyncRequest(
        operation,
        m_url, 
        nameMapper()->name(processor->object()), 
        headers, 
        params, 
        data,
        processor, 
        SLOT(processReply(QnHTTPRawResponse, int))
    );
}

int QnAbstractConnection::sendAsyncGetRequest(int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot) {
    return sendAsyncRequest(QNetworkAccessManager::GetOperation, object, headers, params, QByteArray(), replyTypeName, target, slot);
}

int QnAbstractConnection::sendAsyncGetRequest(int object, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot) {
    return sendAsyncGetRequest(object, m_headers, params, replyTypeName, target, slot);
}

int QnAbstractConnection::sendSyncRequest(int operation, int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QVariant *reply) {
    QnHTTPRawResponse response;
    int status = QnSessionManager::instance()->sendSyncRequest(
        operation,
        m_url,
        nameMapper()->name(object),
        headers,
        params,
        data,
        response
        );
    if (status) {
        return status;
    }

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


void QnAbstractConnection::setExtraHeaders(const QnRequestHeaderList& headers)
{
    m_headers = headers;
}
