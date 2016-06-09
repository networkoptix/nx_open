#ifndef QN_SERIALIZATION_LEXICAL_H
#define QN_SERIALIZATION_LEXICAL_H

#include <cassert>

#include <QtCore/QString>

#include <utils/serialization/serialization.h>

#include "lexical_fwd.h"


class QnLexicalSerializer;

namespace QnLexicalDetail {
    struct StorageInstance { 
        QnSerializerStorage<QnLexicalSerializer> *operator()() const;
    };
} // namespace QJsonDetail


class QnLexicalSerializer: public QnBasicSerializer<QString>, public QnStaticSerializerStorage<QnLexicalSerializer, QnLexicalDetail::StorageInstance> {
    typedef QnBasicSerializer<QString> base_type;
public:
    QnLexicalSerializer(int type): base_type(type) {}
};

template<class T>
class QnDefaultLexicalSerializer: public QnDefaultBasicSerializer<T, QnLexicalSerializer> {};


namespace QnLexical {
    template<class T>
    void serialize(const T &value, QString *target) {
        QnSerialization::serialize(value, target);
    }

    template<class T>
    bool deserialize(const QString &value, T *target) {
        return QnSerialization::deserialize(value, target);
    }

    template<class T>
    QString serialized(const T &value) {
        QString result;
        QnLexical::serialize(value, &result);
        return result;
    }

    template<class T>
    T deserialized(const QString &value, const T &defaultValue = T(), bool *success = NULL) {
        T target;
        bool result = QnLexical::deserialize(value, &target);
        if (success)
            *success = result;
        return result ? target : defaultValue;
    }

} // namespace QnLexical


#endif // QN_SERIALIZATION_LEXICAL_H
