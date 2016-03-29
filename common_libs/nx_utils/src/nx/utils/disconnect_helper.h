
#pragma once

#include <QtCore/QObject>
#include <QtCore/QSharedPointer>

class QnDisconnectHelper;
typedef QSharedPointer<QnDisconnectHelper> QnDisconnectHelperPtr;

/// @brief Disconnects all added connections on destruction
class NX_UTILS_API QnDisconnectHelper
{
public:
    static QnDisconnectHelperPtr create();

    QnDisconnectHelper();

    ~QnDisconnectHelper();

    void add(const QMetaObject::Connection &connection);

private:
    typedef QList<QMetaObject::Connection> ConnectionsList;

    ConnectionsList m_connections;
};

NX_UTILS_API QnDisconnectHelper& operator << (QnDisconnectHelper &target
    , const QMetaObject::Connection &connection);
