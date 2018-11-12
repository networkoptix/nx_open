#ifndef QN_VARIANT_H
#define QN_VARIANT_H

#include <QtCore/QVariant>

template<class T> 
inline T qvariant_cast(const QVariant &value, const T &defaultValue) {
    const int typeId = qMetaTypeId<T>();
    if (typeId == value.userType())
        return *reinterpret_cast<const T *>(value.constData());

    if (typeId < QMetaType::User && value.userType() < QMetaType::User) {
        QVariant copy = value;
        if(copy.convert(typeId))
            return *reinterpret_cast<const T *>(copy.constData());
    }

    return defaultValue;
}

#endif // QN_VARIANT_H
