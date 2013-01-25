#ifndef QN_GADGET_H
#define QN_GADGET_H

#include <QtCore/qobjectdefs.h>

/**
 * Same as <tt>Q_GADGET</tt>, but doesn't trigger MOC, and can be used in namespaces.
 * The only sane use case is for generating metainformation for enums in
 * namespaces.
 */
#define QN_GADGET                                                               \
    extern const QMetaObject staticMetaObject;                                  \
    extern const QMetaObject &getStaticMetaObject();                            \

#endif // QN_GADGET_H

