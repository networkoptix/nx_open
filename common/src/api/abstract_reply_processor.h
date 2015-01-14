#ifndef QN_ABSTRACT_REPLY_PROCESSOR_H
#define QN_ABSTRACT_REPLY_PROCESSOR_H

#include <QtCore/QObject>

#include <utils/common/request_param.h>

#include <rest/server/json_rest_result.h>
#include <utils/common/warnings.h>
#include <utils/serialization/json.h>

#include "abstract_connection.h"

class QnAbstractReplyProcessor: public QObject {
    Q_OBJECT
    typedef QObject base_type;

public:
    QnAbstractReplyProcessor(int object): m_object(object), m_finished(false), m_status(0), m_handle(0) {}
    virtual ~QnAbstractReplyProcessor() {}

    int object() const { return m_object; }

    bool isFinished() const { return m_finished; }
    int status() const { return m_status; }
    int handle() const { return m_handle; }
    QVariant reply() const { return m_reply; }

    using base_type::connect;
    bool connect(const char *signal, QObject *receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection);

public slots:
    virtual void processReply(const QnHTTPRawResponse &response, int handle);

signals:
    void finished(int status, int handle);
    void finished(int status, int handle, const QString &errorString);
    void finished(int status, const QVariant &reply, int handle, const QString &errorString);

protected:
    template<class T, class Derived>
    void emitFinished(Derived *derived, int status, const T &reply, int handle, const QString &errorString = QString()) {
        m_finished = true;
        m_status = status;
        m_handle = handle;
        m_reply = QVariant::fromValue<T>(reply);
        m_errorString = errorString;

        emit derived->finished(status, reply, handle, errorString);
        emit finished(status, m_reply, handle, errorString);
        emit finished(status, handle, errorString);
        emit finished(status, handle);
    }

    template<class Derived>
    void emitFinished(Derived *, int status, int handle, const QString &errorString = QString()) {
        m_finished = true;
        m_status = status;
        m_handle = handle;
        m_reply = QVariant();
        m_errorString = errorString;

        emit finished(status, m_reply, handle, errorString);
        emit finished(status, handle, errorString);
        emit finished(status, handle);
    }

    template<class T, class Derived>
    void processJsonReply(Derived *derived, const QnHTTPRawResponse &response, int handle) {
        int status = response.status;
        QString errorString = QString::fromUtf8(response.errorString);

        T reply;
        if(status == 0) {
            QnJsonRestResult result;
            bool jsonDeserialized = QJson::deserialize(response.data, &result);
            if (!jsonDeserialized || (!result.reply().isNull() && !QJson::deserialize(result.reply(), &reply))) {
#ifdef JSON_REPLY_DEBUG
                qnWarning("Error parsing JSON reply:\n%1\n\n", response.data);
#endif
                status = 1;
            }
            if (jsonDeserialized)
                errorString = result.errorString();
        } else {
#ifdef JSON_REPLY_DEBUG
            qnWarning("Error processing request: %1.", response.errorString);
#endif
        }

        emitFinished(derived, status, reply, handle, errorString);
    }

private:
    int m_object;
    bool m_finished;
    int m_status;
    int m_handle;
    QVariant m_reply;
    QString m_errorString;
};

#endif // QN_ABSTRACT_REPLY_PROCESSOR_H
