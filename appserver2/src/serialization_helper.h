
#ifndef SERIALIZATION_HELPER_H
#define SERIALIZATION_HELPER_H

#include <QByteArray>

#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/transform.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/mpl/bool.hpp>

//namespace ec2
//{

template<class T>
class BinaryStream;

template<>
class BinaryStream<QIODevice> {
};

template<>
class BinaryStream<QByteArray> {
public:
    BinaryStream<QByteArray>(): pos (0) {}

    int write(const void* data, int size) {
        stream.append((const char*) data, size);
        return size;
    }

    int read(void* buffer, int maxSize) {
        int toRead = qMin(stream.size() - pos, maxSize);
        memcpy(buffer, stream.data() + pos, toRead);
        pos += toRead;
        return toRead;
    }
private:
    QByteArray stream;
    int pos;
};


namespace QnBinary {
    template <class T>
    void serialize(qint32 field, BinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(bool field, BinaryStream<T>* binStream) {
        binStream->write(&field, sizeof(field));
    }

    template <class T>
    void serialize(const QString& field, BinaryStream<T>* binStream) {
        // todo: implement me        
    }

    template <class T>
    void deserialize(qint32& field, BinaryStream<T>* binStream) {
        binStream->read(&field, sizeof(field));
    }

    template <class T>
    void deserialize(bool& field, BinaryStream<T>* binStream) {
        binStream->read(&field, sizeof(field));
    }

    template <class T>
    void deserialize(QString& field, BinaryStream<T>* binStream) {
        // todo: implement me        
    }
};

#define QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS(TYPE, FIELD_SEQ, ... /* PREFIX */) \
    template <class T> \
    __VA_ARGS__ void serialize(const TYPE &value, BinaryStream<T> *target) { \
       BOOST_PP_SEQ_FOR_EACH(SERIALIZE_FIELD, ~, FIELD_SEQ) \
    } \
    \
    template <class T> \
    __VA_ARGS__ void deserialize(TYPE &value, BinaryStream<T> *target) { \
       BOOST_PP_SEQ_FOR_EACH(DESERIALIZE_FIELD, ~, FIELD_SEQ) \
    } \


#define SERIALIZE_FIELD(R, D, FIELD) \
    QnBinary::serialize(value.FIELD, target); \

#define DESERIALIZE_FIELD(R, D, FIELD) \
    QnBinary::deserialize(value.FIELD, target);

//}

#endif  //SERIALIZATION_HELPER_H
