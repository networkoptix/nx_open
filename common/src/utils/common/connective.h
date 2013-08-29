#ifndef QN_CONNECTIVE_H
#define QN_CONNECTIVE_H

#include <boost/type_traits/is_base_of.hpp>

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QWeakPointer>

#include "forward.h"

struct Connection {
    Connection(QObject *sender, const char *signal, QObject *receiver, const char *method): sender(sender), signal(signal), receiver(receiver), method(method) {}
    Connection(): signal(NULL), method(NULL) {}

    bool disconnect() {
        if(sender && receiver) {
            return QObject::disconnect(sender.data(), signal, receiver.data(), method);
        } else {
            return false;
        }
    }

    QWeakPointer<QObject> sender;
    const char *signal;
    QWeakPointer<QObject> receiver;
    const char *method;
};

namespace Qn {
    inline QObject *connector(QObject *object) {
        return object;
    }

    template<class T>
    inline QObject *connector(const QScopedPointer<T> &object) {
        return connector(object.data());
    }

    template<class T>
    inline QObject *connector(const QSharedPointer<T> &object) {
        return connector(object.data());
    }

    template<class T>
    inline QObject *connector(const QWeakPointer<T> &object) {
        return connector(object.data());
    }

    inline bool connect(QObject *sender, const char *signal, QObject *receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection, Connection *connection = NULL) {
        if(QObject::connect(sender, signal, receiver, method, type)) {
            if(connection) 
                *connection = Connection(sender, signal, receiver, method);
            return true;
        } else {
            return false;
        }
    }

    inline bool disconnect(QObject *sender, const char *signal, QObject *receiver, const char *method, Connection *connection = NULL) {
        if(QObject::disconnect(sender, signal, receiver, method)) {
            if(connection) 
                *connection = Connection(sender, signal, receiver, method);
            return true;
        } else {
            return false;
        }
    }

    template<class T1, class T2>
    inline bool connect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection, Connection *connection = NULL) {
        return connect(connector(sender), signal, connector(receiver), method, type, connection);
    }

    template<class T1, class T2>
    inline bool disconnect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Connection *connection = NULL) {
        return disconnect(connector(sender), signal, connector(receiver), method, connection);
    }

} // namespace Qn


template<class Base, bool baseIsConnective>
class Connective;


class ConnectiveBase {
public:
    template<class T1, class T2>
    static bool connect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection, Connection *connection = NULL) {
        using Qn::connect; /* Let ADL kick in. */

        return connect(sender, signal, receiver, method, type, connection);
    }

    template<class T1, class T2>
    static bool disconnect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Connection *connection = NULL) {
        using Qn::disconnect; /* Let ADL kick in. */

        return disconnect(sender, signal, receiver, method, connection);
    }

private:
    ConnectiveBase() {}

    template<class Base, bool baseIsConnective>
    friend class ::Connective; /* So that only this class can access our methods. */
};


/**
 * Convenience base class for objects that want to use extensible ADL-based connections.
 * 
 * It replaces <tt>QObject</tt>'s <tt>connect</tt> and <tt>disconnect</tt>
 * methods with drop-in replacements that call into ADL-based implementation.
 */
template<class Base, bool baseIsConnective = boost::is_base_of<ConnectiveBase, Base>::value>
class Connective: public Base, public ConnectiveBase {
public:
    QN_FORWARD_CONSTRUCTOR(Connective, Base, {});

    using ConnectiveBase::connect;
    using ConnectiveBase::disconnect;
};


template<class Base>
class Connective<Base, true>: public Base {
public:
    QN_FORWARD_CONSTRUCTOR(Connective, Base, {});
};


#endif // QN_CONNECTIVE_H
