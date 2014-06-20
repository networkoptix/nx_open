#ifndef QN_SERIALIZATION_H
#define QN_SERIALIZATION_H

#include <cassert>

#include <type_traits> /* For std::enable_if, std::is_base_of, std::integral_constant. */

#ifndef QN_NO_QT
#include <QtCore/QVariant>
#endif

#include <utils/common/conversion_wrapper.h>
#include <utils/common/flat_map.h>
#include <utils/common/synchronized_flat_storage.h>


namespace QnSerializationDetail {
    template<class Context, class T, class D>
    void serialize_direct(Context *ctx, const T &value, D *target);
    template<class Context, class T, class D>
    bool deserialize_direct(Context *ctx, const D &value, T *target);
    template<class T, class D>
    void serialize_internal(const T &value, D *target);
    template<class T, class D>
    bool deserialize_internal(const D &value, T *target);
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

#ifndef QN_NO_QT
    void serialize(context_type *ctx, const QVariant &value, data_type *target) const {
        assert(ctx && value.userType() == type() && target);

        serializeInternal(ctx, value.constData(), target);
    }
#endif
    
    void serialize(context_type *ctx, const void *value, data_type *target) const {
        assert(ctx && value && target);

        serializeInternal(ctx, value, target);
    }

#ifndef QN_NO_QT
    bool deserialize(context_type *ctx, const data_type &value, QVariant *target) const {
        assert(ctx && target);

        *target = QVariant(type(), static_cast<const void *>(NULL));
        return deserializeInternal(ctx, value, target->data());
    }
#endif
    
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
        QnSerializationDetail::serialize_direct(ctx, *static_cast<const T *>(value), target);
    }

    virtual bool deserializeInternal(context_type *ctx, const data_type &value, void *target) const override {
        return QnSerializationDetail::deserialize_direct(ctx, value, static_cast<T *>(target));
    }
};

template<class D>
class QnBasicSerializer: public QnBasicSerializerBase {
public:
    typedef D data_type;

    QnBasicSerializer(int type): QnBasicSerializerBase(type) {}

#ifndef QN_NO_QT
    void serialize(const QVariant &value, data_type *target) const {
        assert(value.userType() == type() && target);

        serializeInternal(value.constData(), target);
    }
#endif
    
    void serialize(const void *value, data_type *target) const {
        assert(value && target);

        serializeInternal(value, target);
    }

#ifndef QN_NO_QT
    bool deserialize(const data_type &value, QVariant *target) const {
        assert(target);

        *target = QVariant(type(), static_cast<const void *>(NULL));
        return deserializeInternal(value, target->data());
    }
#endif
    
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
        QnSerializationDetail::serialize_internal(*static_cast<const T *>(value), target);
    }

    virtual bool deserializeInternal(const data_type &value, void *target) const override {
        return QnSerializationDetail::deserialize_internal(value, static_cast<T *>(target));
    }
};

template<class Serializer>
class QnSerializerStorage {
public:
    Serializer *serializer(int type) {
        return m_storage.value(type);
    }

    QSet<Serializer *> serializers() {
        return m_storage.values();
    }

    void registerSerializer(Serializer *serializer) {
        m_storage.insert(serializer->type(), serializer);
    }

    template<class T>
    void registerSerializer() { 
        registerSerializer(new typename QnSerialization::default_serializer<Serializer, T>::type()); 
    }

private:
    QnSynchronizedFlatStorage<unsigned int, Serializer *> m_storage;
};

template<class Serializer, class Instance>
class QnStaticSerializerStorage {
public:
    static Serializer *serializer(int type) { return Instance()()->serializer(type); }
    static QSet<Serializer *> serializers() { return Instance()()->serializers(); }
    static void registerSerializer(Serializer *serializer) { Instance()()->registerSerializer(serializer); }
    template<class T>
    static void registerSerializer() { registerSerializer(new typename QnSerialization::default_serializer<Serializer, T>::type()); }
};


namespace QnSerializationDetail {

    // TODO: #Elric #ec2 also disable_user_conversions the value?

    /* Internal interface for (de)serializers that do not use context. */

    template<class T, class D>
    void serialize_internal(const T &value, D *target) {
        serialize(value, target); /* That's the place where ADL kicks in. */
    }

    template<class T, class D>
    bool deserialize_internal(const D &value, T *target) {
        /* That's the place where ADL kicks in.
         * 
         * Note that we wrap a serialized type into a wrapper so that
         * ADL would find only overloads with it as the first parameter. 
         * Otherwise other overloads could also be discovered. 
         *
         * Also note that disable_user_conversions is also looked up via ADL, and thus
         * conversion wrapping for the data type can actually be disabled by
         * overloading disable_user_conversions. */
        return deserialize(disable_user_conversions(value), target);
    }

    /* Internal interface for (de)serializers that use context. */

    template<class Context, class T, class D>
    void serialize_direct(Context *ctx, const T &value, D *target) {
        serialize(ctx, value, target); /* That's the place where ADL kicks in. */
    }

    template<class Context, class T, class D>
    bool deserialize_direct(Context *ctx, const D &value, T *target) {
        /* That's the place where ADL kicks in.
         * 
         * Note that we wrap a serialized type into a wrapper so that
         * ADL would find only overloads with it as the first parameter. 
         * Otherwise other overloads could also be discovered. */
        return deserialize(ctx, disable_user_conversions(value), target);
    }


    // TODO: #Elric qMetaTypeId uses atomics for custom types. Maybe introduce local cache?

    template<class T>
    struct is_metatype_defined: 
        std::integral_constant<bool, QMetaTypeId2<T>::Defined>
    {};

    template<class Context, class T, class D>
    void serialize_internal(Context *ctx, const T &value, D *target, typename std::enable_if<is_metatype_defined<T>::value>::type * = NULL) {
        typename Context::serializer_type *serializer = ctx->serializer(qMetaTypeId<T>());
        if(serializer) {
            serializer->serialize(ctx, static_cast<const void *>(&value), target);
        } else {
            serialize_direct(ctx, value, target);
        }
    }

    template<class Context, class T, class D>
    void serialize_internal(Context *ctx, const T &value, D *target, typename std::enable_if<!is_metatype_defined<T>::value>::type * = NULL) {
        serialize_direct(ctx, value, target);
    }

    template<class Context, class T, class D>
    bool deserialize_internal(Context *ctx, const D &value, T *target, typename std::enable_if<is_metatype_defined<T>::value>::type * = NULL) {
        typename Context::serializer_type *serializer = ctx->serializer(qMetaTypeId<T>());
        if(serializer) {
            return serializer->deserialize(ctx, value, static_cast<void *>(target));
        } else {
            return deserialize_direct(ctx, value, target);
        }
    }

    template<class Context, class T, class D>
    bool deserialize_internal(Context *ctx, const D &value, T *target, typename std::enable_if<!is_metatype_defined<T>::value>::type * = NULL) {
        return deserialize_direct(ctx, value, target);
    }

} // namespace QnSerializationDetail

namespace QnSerialization {

    /* Public interface for (de)serializers that do not use context. */

    template<class T, class D>
    void serialize(const T &value, D *target) {
        assert(target);
        QnSerializationDetail::serialize_internal(value, target);
    }

    template<class T, class D>
    bool deserialize(const D &value, T *target) {
        assert(target);
        return QnSerializationDetail::deserialize_internal(value, target);
    }

    
    /* Public interface for (de)serializers that use context. */

    // TODO: use template for context, with static_assert, so that the 
    // actual type is passed down the call tree.

    template<class Context, class T, class D>
    void serialize(Context *ctx, const T &value, D *target) {
        assert(ctx && target);
        QnSerializationDetail::serialize_internal(ctx, value, target);
    }

    template<class Context, class T, class D>
    bool deserialize(Context *ctx, const D &value, T *target) {
        assert(ctx && target);
        return QnSerializationDetail::deserialize_internal(ctx, value, target);
    }

    template<class Serializer, class T>
    struct default_serializer<Serializer, T, typename std::enable_if<std::is_base_of<QnContextSerializerBase, Serializer>::value>::type> {
        typedef QnDefaultContextSerializer<T, Serializer> type;
    };

    template<class Serializer, class T>
    struct default_serializer<Serializer, T, typename std::enable_if<std::is_base_of<QnBasicSerializerBase, Serializer>::value>::type> {
        typedef QnDefaultBasicSerializer<T, Serializer> type;
    };

} // namespace QnSerialization

#endif // QN_SERIALIZATION_H
