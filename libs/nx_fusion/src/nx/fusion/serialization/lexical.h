// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cassert>
#include <string>

#include <QtCore/QString>

#include <nx/fusion/serialization/serialization.h>

#include "enum.h"
#include "lexical_fwd.h"

class QnLexicalSerializer;

namespace QnLexicalDetail {
    struct StorageInstance {
        NX_FUSION_API QnSerializerStorage<QnLexicalSerializer>* operator()() const;
    };
} // namespace QJsonDetail


class QnLexicalSerializer: public QnBasicSerializer<QString>, public QnStaticSerializerStorage<QnLexicalSerializer, QnLexicalDetail::StorageInstance> {
    typedef QnBasicSerializer<QString> base_type;
public:
    QnLexicalSerializer(QMetaType type): base_type(type) {}
};

template<class T>
class QnDefaultLexicalSerializer: public QnDefaultBasicSerializer<T, QnLexicalSerializer> {};


namespace QnLexical {
    template<typename T>
    void serialize(const T& value, QString* target)
    {
        static_assert(!QnSerialization::IsInstrumentedEnumOrFlags<T>::value,
            "Do not use QnLexical for instrumented enums. Use nx::reflect::toString() instead.");
        QnSerialization::serialize(value, target);
    }

    template<typename T>
    bool deserialize(const QString& value, T* target)
    {
        static_assert(!QnSerialization::IsInstrumentedEnumOrFlags<T>::value,
            "Do not use QnLexical for instrumented enums. Use nx::reflect::fromString() instead.");
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

    template<class T, typename String>
    T deserialized(
        const String& value, const T& defaultValue = T(), bool* success = NULL,
        std::enable_if_t<
            std::is_same_v<String, std::string> ||
            std::is_same_v<String, std::string_view>>* = nullptr)
    {
        return deserialized<T>(QString::fromStdString(value), defaultValue, success);
    }

} // namespace QnLexical
