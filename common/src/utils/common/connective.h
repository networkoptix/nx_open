#ifndef QN_CONNECTIVE_H
#define QN_CONNECTIVE_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>
#include <QtCore/QWeakPointer>

#include "forward.h"

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


    template<class T1, class T2>
    inline bool connect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection) {
        return QObject::connect(connector(sender), signal, connector(receiver), method, type);
    }

    template<class T1, class T2>
    inline bool disconnect(const T1 &sender, const char *signal, const T2 &receiver, const char *method) {
        return QObject::disconnect(connector(sender), signal, connector(receiver), method);
    }

    namespace detail {
        template<class T1, class T2>
        inline bool adl_connect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection) {
            using Qn::connect; /* Let ADL kick in. */

            return connect(sender, signal, receiver, method, type);
        }

        template<class T1, class T2>
        inline bool adl_disconnect(const T1 &sender, const char *signal, const T2 &receiver, const char *method) {
            using Qn::disconnect; /* Let ADL kick in. */

            return disconnect(sender, signal, receiver, method);
        }
    }

} // namespace Qn


template<class Base>
class Connective: public Base {
public:
    QN_FORWARD_CONSTRUCTOR(Connective, Base, {});

    template<class T1, class T2>
    static bool connect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection) {
        return Qn::detail::adl_connect(sender, signal, receiver, method, type);
    }

    template<class T1, class T2>
    static bool disconnect(const T1 &sender, const char *signal, const T2 &receiver, const char *method) {
        return Qn::detail::adl_disconnect(sender, signal, receiver, method);
    }
};

#endif // QN_CONNECTIVE_H
