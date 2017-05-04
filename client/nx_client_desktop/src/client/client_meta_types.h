#ifndef QN_CLIENT_META_TYPES_H
#define QN_CLIENT_META_TYPES_H

#include <nx/utils/uuid.h>
#include <QtCore/QVector>
#include <QtCore/QMetaType>
#include <QtGui/QColor>

#include <common/common_meta_types.h>
#include <client_core/client_core_meta_types.h>

/**
 * Convenience class for uniform initialization of metatypes in client module.
 * 
 * Also initializes metatypes from common and client_core module.
 */
class QnClientMetaTypes {
public:
    static void initialize();
};

Q_DECLARE_METATYPE(Qt::KeyboardModifiers)
Q_DECLARE_METATYPE(QVector<QnUuid>)
Q_DECLARE_METATYPE(QVector<QColor>)

#endif // QN_CLIENT_META_TYPES_H
