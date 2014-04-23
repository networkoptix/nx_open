#ifndef QN_SERIALIZATION_H
#define QN_SERIALIZATION_H

#include <cassert>

#ifndef Q_MOC_RUN
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_base_and_derived.hpp>
#endif // Q_MOC_RUN

#include <utils/common/adl_wrapper.h>
#include <utils/common/flat_map.h>
#include <utils/common/synchronized_flat_storage.h>


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

namespace QnSerialization {
    template<class Serializer, class T, class Enable = void>
    struct default_serializer;
} // namespace QnSerialization

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

class QnSerializerBase {
public:
    QnSerializerBase(int type): m_type(type) {}
    virtual ~QnSerializerBase() {}

    int type() const {
        return m_type;
    }

private:
    int m_type;
};

class QnContextSerializerBase: public QnSerializerBase {
public:
    QnContextSerializerBase(int type): QnSerializerBase(type) {}
};

class QnBasicSerializerBase: public QnSerializerBase {
public:
    QnBasicSerializerBase(int type): QnSerializerBase(type) {}
};

template<class D, class Context>
class QnContextSerializer: public QnContextSerializerBase {
public:
    typedef Context context_type;
    typedef D data_type;

    QnContextSerializer(int type): QnContextSerializerBase(type) {}

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

        *target = QVariant(type(), static_cast<const void *>(NULL));
        return deserializeInternal(ctx, value, target->data());
    }

    bool deserialize(context_type *ctx, const data_type &value, void *target) const {
        assert(ctx && target);

        return deserializeInternal(ctx, value, target);
    }

protected:
    virtual void serializeInternal(context_type *ctx, const void *value, data_type *target) const = 0;
    virtual bool deserializeInternal(context_type *ctx, const data_type &value, void *target) const = 0;
};

template<class T, class Base>
class QnDefaultContextSerializer: public Base {
public:
    using typename Base::context_type;
    using typename Base::data_type;

    QnDefaultContextSerializer(): Base(qMetaTypeId<T>()) {}

protected:
    virtual void serializeInternal(context_type *ctx, const void *value, data_type *target) const override {
        QnSerializationDetail::serialize_value_direct(ctx, *static_cast<const T *>(value), target);
    }

    virtual bool deserializeInternal(context_type *ctx, const data_type &value, void *target) const override {
        return QnSerializationDetail::deserialize_value_direct(ctx, value, static_cast<T *>(target));
    }
};

template<class D>
class QnBasicSerializer: public QnBasicSerializerBase {
public:
    typedef D data_type;

    QnBasicSerializer(int type): QnBasicSerializerBase(type) {}

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
};

template<class T, class Base>
class QnDefaultBasicSerializer: public Base {
public:
    using typename Base::data_type;

    QnDefaultBasicSerializer(): Base(qMetaTypeId<T>()) {}

protected:
    virtual void serializeInternal(const void *value, data_type *target) const override {
        QnSerializationDetail::serialize_value(*static_cast<const T *>(value), target);
    }

    virtual bool deserializeInternal(const data_type &value, void *target) const override {
        return QnSerializationDetail::deserialize_value(value, static_cast<T *>(target));
    }
};

template<class Serializer>
class QnSerializerStorage {
public:
    Serializer *serializer(int type) {
        return m_storage.value(type);
    }

    QList<Serializer *> serializers() {
        return m_storage.values();
    }

    void registerSerializer(Serializer *serializer) {
        m_storage.insert(serializer->type(), serializer);
    }

    template<class T>
    void registerSerializer() { 
        registerSerializer(new QnSerialization::default_serializer<Serializer, T>::type()); 
    }

private:
    QnSynchronizedFlatStorage<unsigned int, Serializer *> m_storage;
};

template<class Serializer, class Instance>
class QnStaticSerializerStorage {
public:
    static Serializer *serializer(int type) { return Instance()()->serializer(type); }
    static QList<Serializer *> serializers() { return Instance()()->serializers(); }
    static void registerSerializer(Serializer *serializer) { Instance()()->registerSerializer(serializer); }
    template<class T>
    static void registerSerializer() { registerSerializer(new QnSerialization::default_serializer<Serializer, T>::type()); }
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
         * Otherwise other overloads could also be discovered. 
         *
         * Also note that adl_wrap is also looked up via ADL, and thus
         * ADL wrapping for the data type can actually be disabled by
         * overloading adl_wrap. */
        return deserialize(adl_wrap(value), target);
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
        return deserialize(ctx, adl_wrap(value), target);
    }


    // TODO: #Elric qMetaTypeId uses atomics for custom types. Maybe introduce local cache?

    template<class T>
    struct is_metatype_defined: boost::mpl::bool_<QMetaTypeId2<T>::Defined> {};

    template<class Context, class T, class D>
    void serialize_value(Context *ctx, const T &value, D *target, typename boost::enable_if<is_metatype_defined<T> >::type * = NULL) {
        typename Context::serializer_type *serializer = ctx->serializer(qMetaTypeId<T>());
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
        typename Context::serializer_type *serializer = ctx->serializer(qMetaTypeId<T>());
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

    template<class Serializer, class T>
    struct default_serializer<Serializer, T, typename boost::enable_if<boost::is_base_and_derived<QnContextSerializerBase, Serializer> >::type> {
        typedef QnDefaultContextSerializer<T, Serializer> type;
    };

    template<class Serializer, class T>
    struct default_serializer<Serializer, T, typename boost::enable_if<boost::is_base_and_derived<QnBasicSerializerBase, Serializer> >::type> {
        typedef QnDefaultBasicSerializer<T, Serializer> type;
    };

} // namespace QnSerialization

#endif // QN_SERIALIZATION_H
