#ifndef QN_CLIENT_META_TYPES_H
#define QN_CLIENT_META_TYPES_H

#include <QtCore/QUuid>
#include <QtCore/QVector>
#include <QtCore/QMetaType>

/**
 * Convenience class for uniform initialization of metatypes in client module.
 * 
 * Also initializes metatypes from common module.
 */
class QnClientMetaTypes {
public:
    static void initialize();
};

Q_DECLARE_METATYPE(QVector<QUuid>);

#endif // QN_CLIENT_META_TYPES_H
