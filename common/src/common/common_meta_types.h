#ifndef QN_COMMON_META_TYPES_H
#define QN_COMMON_META_TYPES_H

#include <QtCore/QUuid>
#include <QtCore/QMetaType>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QAuthenticator>


// TODO: #Elric move to model functions?
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
    static void initialize();
};

Q_DECLARE_METATYPE(QHostAddress);
Q_DECLARE_METATYPE(QAuthenticator);
Q_DECLARE_METATYPE(Qt::ConnectionType);
Q_DECLARE_METATYPE(Qt::Orientations);

#endif // QN_COMMON_META_TYPES_H
