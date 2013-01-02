#ifndef QN_ADL_CONNECTIVE_H
#define QN_ADL_CONNECTIVE_H

#include "forward.h"

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedPointer>

namespace Qn {
    QObject *connector(QObject *object) {
        return object;
    }

    template<class T>
    QObject *connector(const QScopedPointer<T> &object) {
        return connector(object.data());
    }

    template<class T>
    QObject *connector(const QSharedPointer<T> &object) {
        return connector(object.data());
    }


    template<class T1, class T2>
    bool connect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection) {
        return QObject::connect(connector(sender), signal, connector(receiver), method, type);
    }

    template<class T1, class T2>
    bool disconnect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type) {
        return QObject::disconnect(connector(sender), signal, connector(receiver), method);
    }

    namespace detail {
        template<class T1, class T2>
        bool adl_connect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection) {
            using Qn::connect; /* Let ADL kick in. */

            return connect(sender, signal, receiver, method, type);
        }

        template<class T1, class T2>
        bool adl_disconnect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type) {
            using Qn::disconnect; /* Let ADL kick in. */

            return disconnect(sender, signal, receiver, method);
        }
    }

} // namespace Qn


template<class Base>
class AdlConnective {
public:
    QN_FORWARD_CONSTRUCTOR(AdlConnective, Base, {});

    using Base::connect;
    using Base::disconnect;

    template<class T1, class T2>
    static bool connect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type = Qt::AutoConnection) {
        return Qn::detail::adl_connect(sender, signal, receiver, method, type);
    }

    template<class T1, class T2>
    static bool disconnect(const T1 &sender, const char *signal, const T2 &receiver, const char *method, Qt::ConnectionType type) {
        return Qn::detail::adl_disconnect(sender, signal, receiver, method);
    }
};

#endif // QN_ADL_CONNECTIVE_H
