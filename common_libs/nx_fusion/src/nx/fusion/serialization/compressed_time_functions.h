#ifndef QN_COMPRESSED_TIME_FUNCTIONS_H
#define QN_COMPRESSED_TIME_FUNCTIONS_H

#include <type_traits> /* For std::enable_if. */
#include <array>
#include <vector>
#include <set>
#include <map>

#ifndef Q_MOC_RUN
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/cat.hpp>
#endif

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QUrl>

#include <nx/utils/collection.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/uuid.h>

#include "collection_fwd.h"
#include "compressed_time.h"
#include "compressed_time_macros.h"
#include "enum.h"


namespace QnCompressedTimeDetail {
    template<class Element, class Output, class Tag>
    void serialize_collection_element(const Element &element, QnCompressedTimeWriter<Output> *stream, const Tag &) {
        QnCompressedTime::serialize(element, stream);
    }

    template<class Element, class Output>
    void serialize_collection_element(const Element &element, QnCompressedTimeWriter<Output> *stream, const QnCollection::map_tag &) {
        QnCompressedTime::serialize(element.first, stream);
        QnCompressedTime::serialize(element.second, stream);
    }

    template<class Collection, class Output>
    void serialize_collection(const Collection &value, QnCompressedTimeWriter<Output> *stream) {
        stream->resetLastValue();
        stream->writeSizeToStream(static_cast<int>(value.size()));
        for(auto pos = boost::begin(value); pos != boost::end(value); ++pos)
            serialize_collection_element(*pos, stream, typename QnCollection::collection_category<Collection>::type());
    }

    template<class Collection, class Input, class Element>
    bool deserialize_collection_element(QnCompressedTimeReader<Input> *stream, Collection *target, const Element *, const QnCollection::list_tag &) {
        return QnCompressedTime::deserialize(stream, &*QnCollection::insert(*target, boost::end(*target), Element()));
    }

    template<class Collection, class Input, class Element>
    bool deserialize_collection_element(QnCompressedTimeReader<Input> *stream, Collection *target, const Element *, const QnCollection::set_tag &) {
        Element element;
        if(!QnCompressedTime::deserialize(stream, &element))
            return false;

        QnCollection::insert(*target, boost::end(*target), std::move(element));
        return true;
    }

    template<class Collection, class Input, class Element>
    bool deserialize_collection_element(QnCompressedTimeReader<Input> *stream, Collection *target, const Element *, const QnCollection::map_tag &) {
        typename Collection::key_type key;
        if(!QnCompressedTime::deserialize(stream, &key))
            return false;

        if(!QnCompressedTime::deserialize(stream, &(*target)[key]))
            return false;

        return true;
    }

    template<class Collection, class Input>
    bool deserialize_collection(QnCompressedTimeReader<Input> *stream, Collection *target) {
        typedef typename std::iterator_traits<typename boost::range_mutable_iterator<Collection>::type>::value_type value_type;

        stream->resetLastValue();

        int size = 0;
        if (!stream->readSizeFromStream(&size))
            return false;

        QnCollection::clear(*target);
        if(size >= 0)
            QnCollection::reserve(*target, size);

        for(; size > 0; --size) {
            if(!deserialize_collection_element(stream, target, static_cast<const value_type *>(NULL), typename QnCollection::collection_category<Collection>::type()))
                return false;
        }

        return true;
    }
} // QnCompressedTimeDetail


#ifndef Q_MOC_RUN
#define QN_DEFINE_DIRECT_COMPRESSED_TIME_SERIALIZATION_FUNCTIONS(TYPE, READ_METHOD, WRITE_METHOD) \
template<class Output>                                                          \
void serialize(const TYPE &value, QnCompressedTimeWriter<Output> *stream) {     \
    stream->WRITE_METHOD(value);                                                \
}                                                                               \
                                                                                \
template<class Input>                                                           \
bool deserialize(QnCompressedTimeReader<Input> *stream, TYPE *target) {         \
    return stream->READ_METHOD(target);                                         \
}

//QN_DEFINE_DIRECT_COMPRESSED_TIME_SERIALIZATION_FUNCTIONS(qint64,        readInt64,      writeInt64)
QN_DEFINE_DIRECT_COMPRESSED_TIME_SERIALIZATION_FUNCTIONS(QnTimePeriod,  readQnTimePeriod, writeQnTimePeriod)
QN_DEFINE_DIRECT_COMPRESSED_TIME_SERIALIZATION_FUNCTIONS(QnUuid,        readQnUuid,       writeQnUuid)
#undef QN_DEFINE_DIRECT_COMPRESSED_TIME_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_COLLECTION_COMPRESSED_TIME_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG, IMPL) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF), class Output>                            \
void serialize(const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QnCompressedTimeWriter<Output> *stream) { \
    QnCompressedTimeDetail::BOOST_PP_CAT(serialize_, IMPL)(value, stream);      \
}                                                                               \
                                                                                \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF), class Input>                             \
bool deserialize(QnCompressedTimeReader<Input> *stream, TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> *target) { \
    return QnCompressedTimeDetail::BOOST_PP_CAT(deserialize_, IMPL)(stream, target);    \
}

QN_DEFINE_COLLECTION_COMPRESSED_TIME_SERIALIZATION_FUNCTIONS(std::vector, (class T, class Allocator), (T, Allocator), collection);
QN_DEFINE_COLLECTION_COMPRESSED_TIME_SERIALIZATION_FUNCTIONS(QVector, (class T), (T), collection);

#undef QN_DEFINE_COLLECTION_COMPRESSED_TIME_SERIALIZATION_FUNCTIONS
#endif // Q_MOC_RUN


#endif // QN_COMPRESSED_TIME_FUNCTIONS_H
