#ifndef QN_CONNECTION_H
#define QN_CONNECTION_H

#include <optional>
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

#endif // QN_CONNECTION_H
