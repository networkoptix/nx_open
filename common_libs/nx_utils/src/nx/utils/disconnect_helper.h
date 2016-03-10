
#pragma once

#include <QtCore/QObject>

class QnDisconnectHelper;
typedef QSharedPointer<QnDisconnectHelper> QnDisconnectHelperPtr;

/// @brief Disconnects all added connections on destruction
class QnDisconnectHelper
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

QnDisconnectHelper& operator << (QnDisconnectHelper &target
    , const QMetaObject::Connection &connection);
