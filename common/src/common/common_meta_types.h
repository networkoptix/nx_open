#ifndef QN_COMMON_META_TYPES_H
#define QN_COMMON_META_TYPES_H

#include <QtCore/QUuid>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QAuthenticator>

/**
 * Convenience macro for generating stream operators of enumerations.
 */
#define QN_DEFINE_ENUM_STREAM_OPERATORS(ENUM)                                   \
inline QDataStream &operator<<(QDataStream &stream, const ENUM &value) {        \
    return stream << static_cast<int>(value);                                   \
}                                                                               \
inline QDataStream &operator>>(QDataStream &stream, ENUM &value) {              \
    int tmp;                                                                    \
    stream >> tmp;                                                              \
    value = static_cast<ENUM>(tmp);                                             \
    return stream;                                                              \
}


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
