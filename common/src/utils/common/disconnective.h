#ifndef QN_DISCONNECTIVE_H
#define QN_DISCONNECTIVE_H

#ifndef Q_MOC_RUN
#include <boost/type_traits/is_base_of.hpp>
#endif

#include <QtCore/QList>

#include "connective.h"

class DisconnectiveBase {
public:
    template<class T1, class S1, class T2, class S2>
    QMetaObject::Connection connect(const T1 &sender, const S1 &signal, const T2 &receiver, const S2 &method, Qt::ConnectionType type = Qt::AutoConnection) {
        QMetaObject::Connection result = ConnectiveBase::connect(sender, signal, receiver, method, type);
        if(result)
            m_connections.push_back(result);
        return result;
    }

    template<class T1, class S1, class T2, class S2>
    bool disconnect(const T1 &sender, const S1 &signal, const T2 &receiver, const S2 &method) {
        return ConnectiveBase::disconnect(sender, signal, receiver, method);
    }

    void disconnectAll() {
        for(const QMetaObject::Connection &connection: m_connections)
            QObject::disconnect(connection);
        m_connections.clear();
    }

protected:
    DisconnectiveBase() {}
    virtual ~DisconnectiveBase() {}

private:
    QList<QMetaObject::Connection> m_connections;
};


/**
 * Convenience class that makes it possible to record all connections made
 * inside a class and then break them all at once.
 * 
 * The connections should be made with a call to <tt>connect</tt> instance method,
 * and can then be broken all at once with a call to <tt>disconnectAll</tt>.
 */
template<class Base, bool baseIsDisconnective = boost::is_base_of<DisconnectiveBase, Base>::value>
class Disconnective: public Connective<Base>, public DisconnectiveBase {
    typedef Connective<Base> base_type;
public:
    QN_FORWARD_CONSTRUCTOR(Disconnective, base_type, {});
    
    using DisconnectiveBase::connect;
    using DisconnectiveBase::disconnect;
    using DisconnectiveBase::disconnectAll;
};


template<class Base>
class Disconnective<Base, true>: public Base {
public:
    QN_FORWARD_CONSTRUCTOR(Disconnective, Base, {});
};


#endif // QN_DISCONNECTIVE_H
