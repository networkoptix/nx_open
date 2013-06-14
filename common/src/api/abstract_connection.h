#ifndef QN_CONNECTION_H
#define QN_CONNECTION_H

#include <cassert>

#include <boost/preprocessor/stringize.hpp>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QScopedPointer>
#include <QtNetwork/QNetworkAccessManager>

#include <utils/common/request_param.h>
#include <utils/common/warnings.h>
#include <utils/common/json.h>
#include <utils/common/connective.h>
#include <utils/common/enum_name_mapper.h>

namespace detail {
    template<class T>
    const char *check_type() { return NULL; }
}

/**
 * Macro that stringizes the given type name and checks at compile time that
 * the given type is actually defined.
 * 
 * \param TYPE                          Type to stringize.
 */
#define QN_STRINGIZE_TYPE(TYPE) (detail::check_type<TYPE>(), BOOST_PP_STRINGIZE(TYPE))



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
    void finished(int status, const QVariant &reply, int handle);

protected:
    template<class T, class Derived>
    void emitFinished(Derived *derived, int status, const T &reply, int handle) {
        m_finished = true;
        m_status = status;
        m_handle = handle;
        m_reply = QVariant::fromValue<T>(reply);

        emit derived->finished(status, reply, handle);
        emit finished(status, m_reply, handle);
    }

    template<class Derived>
    void emitFinished(Derived *, int status, int handle) {
        m_finished = true;
        m_status = status;
        m_handle = handle;
        m_reply = QVariant();

        emit finished(status, handle);
        emit finished(status, m_reply, handle);
    }

    template<class T, class Derived>
    void processJsonReply(Derived *derived, const QnHTTPRawResponse &response, int handle) {
        int status = response.status;

        T reply;
        if(status == 0) {
            QVariantMap map;
            if(!QJson::deserialize(response.data, &map) || !QJson::deserialize(map, "reply", &reply)) {
                qnWarning("Error parsing JSON reply:\n%1\n\n", response.data);
                status = 1;
            }
        } else {
            qnWarning("Error processing request: %1.", response.errorString);
        }

        emitFinished(derived, status, reply, handle);
    }

private:
    int m_object;
    bool m_finished;
    int m_status;
    int m_handle;
    QVariant m_reply;
};



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

private:
    bool m_finished;
    int m_status;
    int m_handle;
    QVariant m_reply;
};



class QnAbstractConnection: public Connective<QObject> {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnAbstractConnection(QObject *parent = NULL);
    virtual ~QnAbstractConnection();

    QUrl url() const;
    void setUrl(const QUrl &url);

protected:
    virtual QnAbstractReplyProcessor *newReplyProcessor(int object) = 0;

    QnEnumNameMapper *nameMapper() const;
    void setNameMapper(QnEnumNameMapper *nameMapper);

    int sendAsyncRequest(int operation, int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, const char *replyTypeName, QObject *target, const char *slot);
    int sendAsyncGetRequest(int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot);
    int sendAsyncGetRequest(int object, const QnRequestParamList &params, const char *replyTypeName, QObject *target, const char *slot);

    int sendSyncRequest(int operation, int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, QVariant *reply);
    int sendSyncGetRequest(int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, QVariant *reply);
    int sendSyncGetRequest(int object, const QnRequestParamList &params, QVariant *reply);

    template<class T>
    int sendSyncRequest(int operation, int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, const QByteArray& data, T *reply) {
        assert(reply);

        QVariant replyVariant;
        int status = sendSyncRequest(operation, object, headers, params, data, &replyVariant);
        
        int replyType = qMetaTypeId<T>();
        if(replyVariant.userType() != replyType)
            qnWarning("Invalid return type of request '%1': expected '%2', got '%3'.", m_nameMapper->name(object), QMetaType::typeName(replyType), replyVariant.typeName());

        *reply = replyVariant.value<T>();
        return status;
    }

    template<class T>
    int sendSyncGetRequest(int object, const QnRequestHeaderList &headers, const QnRequestParamList &params, T *reply) {
        return sendSyncRequest(QNetworkAccessManager::GetOperation, object, headers, params, QByteArray(), reply);
    }

    template<class T>
    int sendSyncGetRequest(int object, const QnRequestParamList &params, T *reply) {
        return sendSyncGetRequest(object, QnRequestHeaderList(), params, reply);
    }

private:
    static bool connectProcessor(QnAbstractReplyProcessor *sender, const char *signal, QObject *receiver, const char *method, Qt::ConnectionType connectionType = Qt::AutoConnection);

private:
    QUrl m_url;
    QScopedPointer<QnEnumNameMapper> m_nameMapper;
};


#endif // QN_CONNECTION_H
