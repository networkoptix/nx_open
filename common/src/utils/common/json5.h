#ifndef JSON5_H
#define JSON5_H

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>

/**
 * Module with new set of serialization methods declared as it suggested in Qt5 documentation.
 */

namespace QJson_detail {
    template<class T>
    void serialize_value(const T &value, QJsonObject &json);

    template<class T>
    bool deserialize_value(const QJsonObject &json, T *target);

} // namespace QJson_detail


namespace QJson {

    template<class T>
    void serialize(const T &value, QJsonObject &json) {
        QJson_detail::serialize_value(value, json);
    }

    template<class T>
    void deserialize(const QJsonObject &json, T *target) {
        QJson_detail::deserialize_value(json, target);
    }

    template<class T>
    QString serialized5(const T &value) {
        QJsonObject json;
        serialize(value, json);
        return QString::fromUtf8(QJsonDocument(json).toJson());
    }

    template<class T>
    QString serializedList(const QList<T> &value) {

        QJsonObject json;
        QJsonArray array;
        foreach (const T &item, value) {
            QJsonObject object;
            serialize(item, object);
            array.append(object);
        }
        json[QLatin1String("array")] = array;

        return QString::fromUtf8(QJsonDocument(json).toJson());
    }

    template<class T>
    T deserialized5(const QString &value) {
        T result;
        deserialize(QJsonDocument::fromJson(value.toUtf8()).object(), &result);
        return result;
    }

    template<class T>
    QList<T> deserializedList(const QString &value) {
        QJsonObject json = QJsonDocument::fromJson(value.toUtf8()).object();
        QJsonArray array = json[QLatin1String("array")].toArray();

        QList<T> result;
        for (int index = 0; index < array.size(); ++index) {
            T item;
            QJsonObject object = array[index].toObject();
            deserialize(object, &item);
            result << item;
        }
        return result;
    }
}

namespace QJson_detail {
    template<class T>
    void serialize_value(const T &value, QJsonObject &json) {
        serialize(value, json); /* That's the place where ADL kicks in. */
    }

    template<class T>
    bool deserialize_value(const QJsonObject &json, T *target) {
        return deserialize(json, target); /* That's the place where ADL kicks in. */
    }
}

#define QN_DECLARE_SERIALIZATION_FUNCTIONS_5(TYPE, ... /* PREFIX */)            \
__VA_ARGS__ void serialize(const TYPE &value, QJsonObject &json);               \
__VA_ARGS__ bool deserialize(const QJsonObject &json, TYPE *target);


#endif // JSON5_H
