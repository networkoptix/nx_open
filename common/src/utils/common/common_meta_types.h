#ifndef QN_COMMON_META_TYPES_H
#define QN_COMMON_META_TYPES_H

#include <QtCore/QUuid>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QAuthenticator>

/**
 * Convenience class for uniform initialization of metatypes in common module.
 */
class QnCommonMetaTypes {
public:
    static void initilize();
};

Q_DECLARE_METATYPE(QUuid);
Q_DECLARE_METATYPE(QHostAddress);
Q_DECLARE_METATYPE(QAuthenticator);

#endif // QN_COMMON_META_TYPES_H
