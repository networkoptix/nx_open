#include "abstract_connection.h"

#include <cstring> /* For std::strstr. */

#include <utils/common/enum_name_mapper.h>

#include "session_manager.h"

Q_GLOBAL_STATIC(QnEnumNameMapper, qn_abstractConnection_emptyNameMapper);

QnAbstractConnection::QnAbstractConnection(QObject *parent): 
    QObject(parent)
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

int QnAbstractConnection::sendAsyncRequest(int object, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot) {
    QnAbstractReplyProcessor *processor = newReplyProcessor(object);

    QByteArray signal;
    if(replyTypeName == NULL) {
        signal = SIGNAL(finished(int, int));
    } else {
        signal = lit("%1finished(int, const %2 &, int)").arg(QSIGNAL_CODE).arg(QLatin1String(replyTypeName)).toLatin1();
    }
    connectProcessor(processor, signal.constData(), target, slot, Qt::QueuedConnection);

    return QnSessionManager::instance()->sendAsyncGetRequest(
        m_url, 
        nameMapper()->name(processor->object()), 
        QnRequestHeaderList(), 
        params, 
        processor, 
        SLOT(processReply(QnHTTPRawResponse, int))
    );
}

int QnAbstractConnection::sendSyncRequest(int object, const QnRequestParamList &params, QVariant *reply) {
    assert(reply);

    QnHTTPRawResponse response;
    QnSessionManager::instance()->sendGetRequest(
        m_url,
        nameMapper()->name(object),
        QnRequestHeaderList(),
        params,
        response
    );

    QScopedPointer<QnAbstractReplyProcessor> processor(newReplyProcessor(object));
    processor->processReply(response, -1);
    *reply = processor->reply();

    return processor->status();
}

bool QnAbstractConnection::connectProcessor(QnAbstractReplyProcessor *sender, const char *signal, QObject *receiver, const char *method, Qt::ConnectionType connectionType) {
    if(std::strstr(method, "QVariant")) {
        return base_type::connect(sender, SIGNAL(finished(int, const QVariant &, int)), receiver, method, connectionType);
    } else {
        return base_type::connect(sender, signal, receiver, method, connectionType);
    }
}
