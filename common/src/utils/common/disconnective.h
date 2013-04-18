#ifndef QN_DISCONNECTIVE_H
#define QN_DISCONNECTIVE_H

#include <boost/type_traits/is_base_of.hpp>

#include <QtCore/QList>

#include "connective.h"

template<class Base, bool baseIsDisconnective>
class Disconnective;


class DisconnectiveBase {
private:
    DisconnectiveBase() {}

    template<class Base, bool baseIsDisconnective>
    friend class ::Disconnective; /* So that only this class can access our methods. */

private:
    QList<Connection> m_connections;
};


template<class Base, bool baseIsDisconnective = boost::is_base_of<DisconnectiveBase, Base>::value>
class Disconnective: public Connective<Base>, public DisconnectiveBase {
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
};


template<class Base>
class Disconnective<Base, true>: public Base {
public:
    QN_FORWARD_CONSTRUCTOR(Disconnective, Base, {});
};


#endif // QN_DISCONNECTIVE_H
