#include "abstract_connection.h"

#include <QtCore/QUrlQuery>

#include <cstring> /* For std::strstr. */

#include <api/network_proxy_factory.h>
#include <core/resource/resource.h>
#include <nx/fusion/serialization/lexical_enum.h>

#include "session_manager.h"
#include "abstract_reply_processor.h"
#include <common/common_module.h>

Q_GLOBAL_STATIC(QnEnumLexicalSerializer<int>, qn_abstractConnection_emptySerializer);

void QnAbstractReplyProcessor::processReply(const QnHTTPRawResponse& /*response*/, int /*handle*/)
{
}

bool QnAbstractReplyProcessor::connect(const char *signal, QObject *receiver, const char *method, Qt::ConnectionType type)
{
    if(method && std::strstr(method, "QVariant")) {
        return connect(this, SIGNAL(finished(int, const QVariant &, int, const QString &)), receiver, method, type);
    } else {
        return connect(this, signal, receiver, method, type);
    }
}

QnAbstractConnection::QnAbstractConnection(
    QnCommonModule* commonModule,
    const QnResourcePtr& targetRes)
    :
    base_type(),
    QnCommonModuleAware(commonModule),
    m_targetRes(targetRes)
{}

QnAbstractConnection::~QnAbstractConnection() {
    return;
}

const nx::network::http::HttpHeaders& QnAbstractConnection::extraHeaders() const {
    return m_extraHeaders;
}

void QnAbstractConnection::setExtraHeaders(nx::network::http::HttpHeaders extraHeaders) {
    m_extraHeaders = std::move(extraHeaders);
}

const QnRequestParamList &QnAbstractConnection::extraQueryParameters() const {
    return m_extraQueryParameters;
}

void QnAbstractConnection::setExtraQueryParameters(const QnRequestParamList &extraQueryParameters) {
    m_extraQueryParameters = extraQueryParameters;
}

nx::utils::Url QnAbstractConnection::url() const {
    return m_url;
}

void QnAbstractConnection::setUrl(const nx::utils::Url& url)
{
    m_url = url;
    NX_ASSERT(!url.host().isEmpty());
    NX_ASSERT(url.isValid());
}

QnLexicalSerializer *QnAbstractConnection::serializer() const {
    return m_serializer.data() ? m_serializer.data() : qn_abstractConnection_emptySerializer();
}

void QnAbstractConnection::setSerializer(QnLexicalSerializer *serializer) {
    NX_ASSERT(serializer->type() == QMetaType::Int);

    m_serializer.reset(serializer);
}

QString QnAbstractConnection::objectName(int object) const {
    QString result;
    serializer()->serialize(object, &result);
    return result;
}

int QnAbstractConnection::sendAsyncRequest(
    nx::network::http::Method::ValueType method,
    int object,
    nx::network::http::HttpHeaders headers,
    const QnRequestParamList &params,
    QByteArray msgBody,
    const char *replyTypeName,
    QObject *target,
    const char *slot,
    std::optional<std::chrono::milliseconds> timeout)
{
    if (!isReady())
        return -1;
    if (!m_url.isValid() || m_url.host().isEmpty())
        return -1;

    NX_ASSERT(commonModule(), Q_FUNC_INFO, "Session manager object must exist here");
    if (!commonModule())
        return -1;

    QnAbstractReplyProcessor *processor = nullptr;

    if (target && slot) {
        QByteArray signal;
        if(replyTypeName == NULL) {
            signal = SIGNAL(finished(int, int, QString));
        } else {
            signal = lit("%1finished(int, const %2 &, int, const QString &)").arg(QSIGNAL_CODE).arg(QLatin1String(replyTypeName)).toLatin1();
        }
        processor = newReplyProcessor(object, m_targetRes ? m_targetRes->getId().toString() : QString());
        processor->connect(signal.constData(), target, slot, Qt::QueuedConnection);
    }

    nx::network::http::HttpHeaders actualHeaders = std::move(headers);
    if (!m_extraHeaders.empty())
        std::copy(
            m_extraHeaders.begin(),
            m_extraHeaders.end(),
            std::inserter(actualHeaders, actualHeaders.end()));

    QUrlQuery urlQuery(m_url.toQUrl());
    for (auto it = m_extraQueryParameters.begin(); it != m_extraQueryParameters.end(); ++it)
        urlQuery.addQueryItem(it->first, it->second);
    nx::utils::Url url = m_url;
    url.setQuery(urlQuery);

    return commonModule()->sessionManager()->sendAsyncRequest(
        std::move(method),
        url,
        objectName(object),
        std::move(actualHeaders),
        params,
        std::move(msgBody),
        processor,
        "processReply",
        Qt::AutoConnection,
        timeout);
}

int QnAbstractConnection::sendAsyncGetRequest(
    int object, nx::network::http::HttpHeaders headers, const QnRequestParamList &params,
    const char *replyTypeName, QObject *target, const char *slot,
    std::optional<std::chrono::milliseconds> timeout)
{
    return sendAsyncRequest(
        nx::network::http::Method::get,
        object,
        std::move(headers),
        params,
        QByteArray(),
        replyTypeName,
        target,
        slot,
        timeout);
}

int QnAbstractConnection::sendAsyncGetRequest(
    int object, const QnRequestParamList &params, const char *replyTypeName, QObject *target,
    const char *slot, std::optional<std::chrono::milliseconds> timeout)
{
    return sendAsyncGetRequest(
        object, nx::network::http::HttpHeaders(), params, replyTypeName, target, slot, timeout);
}

int QnAbstractConnection::sendAsyncPostRequest(
    int object, nx::network::http::HttpHeaders headers, const QnRequestParamList &params, QByteArray msgBody,
    const char *replyTypeName, QObject *target, const char *slot,
    std::optional<std::chrono::milliseconds> timeout)
{
    return sendAsyncRequest(
        nx::network::http::Method::post, object, std::move(headers), params,
        std::move(msgBody), replyTypeName, target, slot, timeout);
}

int QnAbstractConnection::sendAsyncPostRequest(
    int object, const QnRequestParamList &params, QByteArray msgBody, const char *replyTypeName,
    QObject *target, const char *slot, std::optional<std::chrono::milliseconds> timeout)
{
    return sendAsyncPostRequest(
        object, nx::network::http::HttpHeaders(), params, std::move(msgBody),
        replyTypeName, target, slot, timeout);
}

int QnAbstractConnection::sendSyncRequest(
    nx::network::http::Method::ValueType method, int object, nx::network::http::HttpHeaders actualHeaders,
    const QnRequestParamList &params, QByteArray msgBody, QVariant *reply,
    std::optional<std::chrono::milliseconds> timeout)
{
    NX_ASSERT(commonModule(), Q_FUNC_INFO, "Session manager object must exist here");
    if (!commonModule())
        return -1;

    if (!m_extraHeaders.empty())
        std::copy(
            m_extraHeaders.begin(),
            m_extraHeaders.end(),
            std::inserter(actualHeaders, actualHeaders.end()));

    QnHTTPRawResponse response;
    int status = commonModule()->sessionManager()->sendSyncRequest(
        method,
        m_url,
        objectName(object),
        std::move(actualHeaders),
        params,
        std::move(msgBody),
        response,
        timeout);
    if (status != 0)
        return status;

    QScopedPointer<QnAbstractReplyProcessor> processor(newReplyProcessor(
        object,
        m_targetRes ? m_targetRes->getId().toString() : QString()));
    processor->processReply(response, -1);
    if(reply)
        *reply = processor->reply();

    return processor->status();
}

int QnAbstractConnection::sendSyncGetRequest(
    int object,
    nx::network::http::HttpHeaders headers,
    const QnRequestParamList &params,
    QVariant *reply,
    std::optional<std::chrono::milliseconds> timeout)
{
    return sendSyncRequest(
        nx::network::http::Method::get, object,
        std::move(headers), params,
        QByteArray(), reply, timeout);
}

int QnAbstractConnection::sendSyncGetRequest(
    int object, const QnRequestParamList &params, QVariant *reply,
    std::optional<std::chrono::milliseconds> timeout)
{
    return sendSyncGetRequest(object, nx::network::http::HttpHeaders(), params, reply, timeout);
}

int QnAbstractConnection::sendSyncPostRequest(
    int object,
    nx::network::http::HttpHeaders headers,
    const QnRequestParamList &params,
    QByteArray msgBody,
    QVariant *reply,
    std::optional<std::chrono::milliseconds> timeout)
{
    return sendSyncRequest(
        nx::network::http::Method::post, object, std::move(headers),
        params, std::move(msgBody), reply, timeout);
}

int QnAbstractConnection::sendSyncPostRequest(
    int object, const QnRequestParamList &params, QByteArray msgBody, QVariant *reply,
    std::optional<std::chrono::milliseconds> timeout)
{
    return sendSyncPostRequest(
        object, nx::network::http::HttpHeaders(), params, std::move(msgBody), reply, timeout);
}

QnResourcePtr QnAbstractConnection::targetResource() const
{
    return m_targetRes;
}

bool QnAbstractConnection::isReady() const
{
    return true;
}
