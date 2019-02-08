#ifndef QN_SERIALIZATION_UBJSON_H
#define QN_SERIALIZATION_UBJSON_H

#include "ubjson_fwd.h"
#include "ubjson_reader.h"
#include "ubjson_writer.h"
#include "serialization.h"

namespace QnUbjson {
    template<class T, class Output>
    void serialize(const T &value, QnUbjsonWriter<Output> *stream) {
        QnSerialization::serialize(value, stream);
    }

    template<class T, class Input>
    bool deserialize(QnUbjsonReader<Input> *stream, T *target) {
        return QnSerialization::deserialize(stream, target);
    }

    template<class T>
    void serialize(const T &value, QByteArray *target) {
        QnUbjsonWriter<QByteArray> stream(target);
        QnUbjson::serialize(value, &stream);
    }

    template<class T>
    QByteArray serialized(const T &value) {
        QByteArray result;
        QnUbjsonWriter<QByteArray> stream(&result);
        QnUbjson::serialize(value, &stream);
        return result;
    }

    template<typename T>
    auto deserialized(const QByteArray& value, T&& defaultValue = {}, bool* success = nullptr)
    {
        using Type = std::remove_reference_t<T>;

        Type target;
        QnUbjsonReader<QByteArray> stream(&value);
        const bool result = QnUbjson::deserialize(&stream, &target);
        if (success)
            *success = result;

        if (result)
            return target;

        return std::forward<Type>(defaultValue);
    }

} // namespace QnUbjson


#endif // QN_SERIALIZATION_UBJSON_H
