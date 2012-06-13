#ifndef QN_VARIANT_H
#define QN_VARIANT_H

#include <QtCore/QVariant>

template<class T> 
inline T qvariant_cast(const QVariant &v, const T &defaultValue) {
    /* Code is copied from Qt. See QVariant implementation. */

    const int vid = qMetaTypeId<T>(static_cast<T *>(0));
    if (vid == v.userType())
        return *reinterpret_cast<const T *>(v.constData());
    if (vid < int(QMetaType::User)) {
        T t;
        if (qvariant_cast_helper(v, QVariant::Type(vid), &t))
            return t;
    }
    return defaultValue;
}

#endif // QN_VARIANT_H
