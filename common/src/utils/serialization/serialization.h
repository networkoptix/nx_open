#ifndef QN_SERIALIZATION_H
#define QN_SERIALIZATION_H

#include <cassert>

#include <boost/mpl/bool.hpp>
#include <boost/utility/enable_if.hpp>

#include <utils/common/adl_wrapper.h>
#include <utils/common/flat_map.h>

// TODO: 
// Qss -> QnSerialization
// Context -> QnSerializationContext
// Serializer -> Qn[Basic|Context]Serializer

namespace QnSerializationDetail {
    template<class Context, class T, class D>
    void serialize_value_direct(Context *ctx, const T &value, D *target);
    template<class Context, class T, class D>
    bool deserialize_value_direct(Context *ctx, const D &value, T *target);
    template<class T, class D>
    void serialize_value(const T &value, D *target);
    template<class T, class D>
    bool deserialize_value(const D &value, T *target);
} // namespace QssDetail


template<class Serializer>
class QnSerializationContext {
public:
    typedef Serializer serializer_type;

    void registerSerializer(serializer_type *serializer) {
        m_serializerByType.insert(serializer->type(), serializer);
    }

    void unregisterSerializer(int type) {
        m_serializerByType.remove(type);
    }

    serializer_type *serializer(int type) const {
        return m_serializerByType.value(type);
    }

private:
    QnFlatMap<unsigned int, serializer_type *> m_serializerByType;
};


template<class D, class Context>
class QnContextSerializer {
public:
    typedef Context context_type;
    typedef D data_type;

    QnContextSerializer(int type): m_type(type) {}
    virtual ~QnContextSerializer() {}

    int type() const {
        return m_type;
    }

    void serialize(context_type *ctx, const QVariant &value, data_type *target) const {
        assert(ctx && value.userType() == m_type && target);

        serializeInternal(ctx, value.constData(), target);
    }

    void serialize(context_type *ctx, const void *value, data_type *target) const {
        assert(ctx && value && target);

        serializeInternal(ctx, value, target);
    }

    bool deserialize(context_type *ctx, const data_type &value, QVariant *target) const {
        assert(ctx && target);

        *target = QVariant(m_type, static_cast<const void *>(NULL));
        return deserializeInternal(ctx, value, target->data());
    }

    bool deserialize(context_type *ctx, const data_type &value, void *target) const {
        assert(ctx && target);

        return deserializeInternal(ctx, value, target);
    }

protected:
    virtual void serializeInternal(context_type *ctx, const void *value, data_type *target) const = 0;
    virtual bool deserializeInternal(context_type *ctx, const data_type &value, void *target) const = 0;

private:
    int m_type;
};


template<class D>
class QnBasicSerializer {
public:
    typedef D data_type;

    QnBasicSerializer(int type): m_type(type) {}
    virtual ~QnBasicSerializer() {}

    int type() const {
        return m_type;
    }

    void serialize(const QVariant &value, data_type *target) const {
        assert(value.userType() == m_type && target);

        serializeInternal(value.constData(), target);
    }

    void serialize(const void *value, data_type *target) const {
        assert(value && target);

        serializeInternal(value, target);
    }

    bool deserialize(const data_type &value, QVariant *target) const {
        assert(target);

        *target = QVariant(m_type, static_cast<const void *>(NULL));
        return deserializeInternal(value, target->data());
    }

    bool deserialize(const data_type &value, void *target) const {
        assert(target);

        return deserializeInternal(value, target);
    }

protected:
    virtual void serializeInternal(const void *value, data_type *target) const = 0;
    virtual bool deserializeInternal(const data_type &value, void *target) const = 0;

private:
    int m_type;
};


template<class T, class Base>
class QnDefaultContextSerializer: public Base {
public:
    using Base::context_type;
    using Base::data_type;

    QnDefaultContextSerializer(): Base(qMetaTypeId<T>()) {}

protected:
    virtual void serializeInternal(context_type *ctx, const void *value, data_type *target) const override {
        QnSerializationDetail::serialize_value_direct(ctx, *static_cast<const T *>(value), target);
    }

    virtual bool deserializeInternal(context_type *ctx, const data_type &value, void *target) const override {
        return QnSerializationDetail::deserialize_value_direct(ctx, value, static_cast<T *>(target));
    }
};

template<class T, class Base>
class QnDefaultBasicSerializer: public Base {
public:
    using Base::data_type;

    QnDefaultBasicSerializer(): Base(qMetaTypeId<T>()) {}

protected:
    virtual void serializeInternal(const void *value, data_type *target) const override {
        QnSerializationDetail::serialize_value(*static_cast<const T *>(value), target);
    }

    virtual bool deserializeInternal(const data_type &value, void *target) const override {
        return QnSerializationDetail::deserialize_value(value, static_cast<T *>(target));
    }
};


namespace QnSerializationDetail {

    /* Internal interface for (de)serializers that do not use context. */

    template<class T, class D>
    void serialize_value(const T &value, D *target) {
        serialize(value, target); /* That's the place where ADL kicks in. */
    }

    template<class T, class D>
    bool deserialize_value(const D &value, T *target) {
        /* That's the place where ADL kicks in.
         * 
         * Note that we wrap a serialized type into a wrapper so that
         * ADL would find only overloads with it as the first parameter. 
         * Otherwise other overloads could also be discovered. */
        return deserialize(adlWrap(value), target);
    }

    /* Internal interface for (de)serializers that use context. */

    template<class Context, class T, class D>
    void serialize_value_direct(Context *ctx, const T &value, D *target) {
        serialize(ctx, value, target); /* That's the place where ADL kicks in. */
    }

    template<class Context, class T, class D>
    bool deserialize_value_direct(Context *ctx, const D &value, T *target) {
        /* That's the place where ADL kicks in.
         * 
         * Note that we wrap a json value into a wrapper so that
         * ADL would find only overloads with QJsonValue as the first parameter. 
         * Otherwise other overloads could be discovered. */
        return deserialize(ctx, adlWrap(value), target);
    }


    // TODO: #Elric qMetaTypeId uses atomics for custom types. Maybe introduce local cache?

    template<class T>
    struct is_metatype_defined: boost::mpl::bool_<QMetaTypeId2<T>::Defined> {};

    template<class Context, class T, class D>
    void serialize_value(Context *ctx, const T &value, D *target, typename boost::enable_if<is_metatype_defined<T> >::type * = NULL) {
        Qss::Serializer<D> *serializer = ctx->serializer(qMetaTypeId<T>());
        if(serializer) {
            serializer->serialize(ctx, static_cast<const void *>(&value), target);
        } else {
            serialize_value_direct(ctx, value, target);
        }
    }

    template<class Context, class T, class D>
    void serialize_value(Context *ctx, const T &value, D *target, typename boost::disable_if<is_metatype_defined<T> >::type * = NULL) {
        serialize_value_direct(ctx, value, target);
    }

    template<class Context, class T, class D>
    bool deserialize_value(Context *ctx, const D &value, T *target, typename boost::enable_if<is_metatype_defined<T> >::type * = NULL) {
        Qss::Serializer<D> *serializer = ctx->serializer(qMetaTypeId<T>());
        if(serializer) {
            return serializer->deserialize(ctx, value, static_cast<void *>(target));
        } else {
            return deserialize_value_direct(ctx, value, target);
        }
    }

    template<class Context, class T, class D>
    bool deserialize_value(Context *ctx, const D &value, T *target, typename boost::disable_if<is_metatype_defined<T> >::type * = NULL) {
        return deserialize_value_direct(ctx, value, target);
    }

} // namespace QnSerializationDetail

namespace QnSerialization {

    /* Public interface for (de)serializers that do not use context. */

    template<class T, class D>
    void serialize(const T &value, D *target) {
        assert(target);
        QnSerializationDetail::serialize_value(value, target);
    }

    template<class T, class D>
    bool deserialize(const D &value, T *target) {
        assert(target);
        return QnSerializationDetail::deserialize_value(value, target);
    }

    
    /* Public interface for (de)serializers that use context. */

    // TODO: use template for context, with static_assert, so that the 
    // actual type is passed down the call tree.

    template<class Context, class T, class D>
    void serialize(Context *ctx, const T &value, D *target) {
        assert(ctx && target);
        QnSerializationDetail::serialize_value(ctx, value, target);
    }

    template<class Context, class T, class D>
    bool deserialize(Context *ctx, const D &value, T *target) {
        assert(ctx && target);
        return QnSerializationDetail::deserialize_value(ctx, value, target);
    }


} // namespace Qss

#endif // QN_SERIALIZATION_H
