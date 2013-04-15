#ifndef QN_CONNECTION_LIST_H
#define QN_CONNECTION_LIST_H

#include <QtCore/QList>

#include "connective.h"

template<class Base>
class Disconnective: public Connective<Base> {
    typedef Connective<Base> base_type;
public:
    QN_FORWARD_CONSTRUCTOR(Disconnective, base_type, {});

    template<class T1, class T2>
    bool connect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection) {
        Connection connection;
        if(base_type::connect(sender, signal, receiver, method, type, &connection)) {
            m_connections.push_back(connection);
            return true;
        } else {
            return false;
        }
    }

    template<class T1, class T2>
    bool disconnect(const T1 &sender, const char *signal, const T2 &receiver, const char *method) {
        return base_type::disconnect(sender, signal, receiver, method, NULL);
    }

    void disconnectAll() {
        foreach(const Connection &connection, m_connections)
            if(connection.sender && connection.receiver)
                disconnect(connection.sender, connection.signal, connection.receiver, connection.method);
        m_connections.clear();
    }

private:
    QList<Connection> m_connections;
};

#endif // QN_CONNECTION_LIST_H
