#ifndef QN_CONNECTIVE_H
#define QN_CONNECTIVE_H

#ifndef Q_MOC_RUN
#include <boost/type_traits/is_base_of.hpp>
#endif

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QPointer>

#include "forward.h"
#include "unused.h"

/* Properly defined NULL is required for this class to work, so we just redefine 
 * it here. Defining it only in config.h doesn't work for some compilers. */
#undef NULL
#define NULL nullptr

namespace Qn {
    template<class T>
    inline const T *connector(const T *object) {
        return object;
    }

    template<class T>
    inline const T *connector(const QScopedPointer<T> &object) {
        return connector(object.data());
    }

    template<class T>
    inline const T *connector(const QSharedPointer<T> &object) {
        return connector(object.data());
    }

    template<class T>
    inline const T *connector(const QPointer<T> &object) {
        return connector(object.data());
    }

    template<class T>
    inline const T *connector(const std::shared_ptr<T> &object) {
        return connector(object.get());
    }

    template<class T1, class S1, class T2, class S2>
    inline QMetaObject::Connection connect(const T1 *sender, const S1 &signal, const T2 *receiver, const S2 &method, Qt::ConnectionType type = Qt::AutoConnection) {
        return QObject::connect(sender, signal, receiver, method, type);
    }

    template<class T1, class S1, class T2, class S2>
    inline bool disconnect(const T1 *sender, const S1 &signal, const T2 *receiver, const S2 &method) {
        unused(method, signal); /* Silence the spurious MSVC warning. */
        return QObject::disconnect(sender, signal, receiver, method);
    }

    template<class T1, class S1, class T2, class S2>
    inline QMetaObject::Connection connect(const T1 &sender, const S1 &signal, const T2 &receiver, const S2 &method, Qt::ConnectionType type = Qt::AutoConnection) {
        return connect(connector(sender), signal, connector(receiver), method, type);
    }

    template<class T1, class S1, class T2, class S2>
    inline bool disconnect(const T1 &sender, const S1 &signal, const T2 &receiver, const S2 &method) {
        return disconnect(connector(sender), signal, connector(receiver), method);
    }

} // namespace Qn


class ConnectiveBase {
public:
    template<class T1, class S1, class T2, class S2>
    static QMetaObject::Connection connect(const T1 &sender, const S1 &signal, const T2 &receiver, const S2 &method, Qt::ConnectionType type = Qt::AutoConnection) {
        using Qn::connect; /* Let ADL kick in. */

        return connect(sender, signal, receiver, method, type);
    }

    template<class T1, class S1, class T2, class S2>
    static bool disconnect(const T1 &sender, const S1 &signal, const T2 &receiver, const S2 &method) {
        using Qn::disconnect; /* Let ADL kick in. */

        return disconnect(sender, signal, receiver, method);
    }

    static bool disconnect(const QMetaObject::Connection &connection) {
        return QObject::disconnect(connection);
    }

protected:
    ConnectiveBase() {}
    virtual ~ConnectiveBase() {}
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
