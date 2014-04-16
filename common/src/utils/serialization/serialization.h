#ifndef QN_SERIALIZATION_H
#define QN_SERIALIZATION_H

#include <cassert>

#include <boost/mpl/bool.hpp>
#include <boost/utility/enable_if.hpp>

#include <utils/common/adl_wrapper.h>
#include <utils/common/flat_map.h>


namespace Qss {
    template<class D>
    class Context;
    template<class D>
    class Serializer;
} // namespace Qss


namespace QssDetail {
    template<class T, class D>
    void serialize_value_direct(Qss::Context<D> *ctx, const T &value, D *target);
    template<class T, class D>
    bool deserialize_value_direct(Qss::Context<D> *ctx, const D &value, T *target);
} // namespace QssDetail


namespace Qss {
    template<class D>
    class Context {
    public:
        void registerSerializer(Serializer<D> *serializer) {
            m_serializerByType.insert(serializer->type(), serializer);
        }

        void unregisterSerializer(int type) {
            m_serializerByType.remove(type);
        }

        Serializer<D> *serializer(int type) const {
            return m_serializerByType.value(type);
        }

    private:
        QnFlatMap<unsigned int, Serializer<D> *> m_serializerByType;
    };


    template<class D>
    class Serializer { // TODO: ContextSerializer & BasicSerializer
    public:
        Serializer(int type): m_type(type) {}
        virtual ~Serializer() {}

        int type() const {
            return m_type;
        }

        void serialize(Context<D> *ctx, const QVariant &value, D *target) const {
            assert(value.userType() == m_type && target);

            serializeInternal(ctx, value.constData(), target);
        }

        void serialize(Context<D> *ctx, const void *value, D *target) const {
            assert(value && target);

            serializeInternal(ctx, value, target);
        }

        bool deserialize(Context<D> *ctx, const D &value, QVariant *target) const {
            assert(target);

            *target = QVariant(m_type, static_cast<const void *>(NULL));
            return deserializeInternal(ctx, value, target->data());
        }

        bool deserialize(Context<D> *ctx, const D &value, void *target) const {
            assert(target);

            return deserializeInternal(ctx, value, target);
        }

    protected:
        virtual void serializeInternal(Context<D> *ctx, const void *value, D *target) const = 0;
        virtual bool deserializeInternal(Context<D> *ctx, const D &value, void *target) const = 0;

    private:
        int m_type;
    };


    template<class T, class D>
    class DefaultSerializer: public Serializer<D> {
    public:
        DefaultSerializer(): Serializer(qMetaTypeId<T>()) {}

    protected:
        virtual void serializeInternal(Context<D> *ctx, const void *value, D *target) const override {
            QssDetail::serialize_value_direct(ctx, *static_cast<const T *>(value), target);
        }

        virtual bool deserializeInternal(Context<D> *ctx, const D &value, void *target) const override {
            return QssDetail::deserialize_value_direct(ctx, value, static_cast<T *>(target));
        }
    };

} // namespace Qss


namespace QssDetail {

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

    template<class T, class D>
    void serialize_value_direct(Qss::Context<D> *ctx, const T &value, D *target) {
        serialize(ctx, value, target); /* That's the place where ADL kicks in. */
    }

    template<class T, class D>
    bool deserialize_value_direct(Qss::Context<D> *ctx, const D &value, T *target) {
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

    template<class T, class D>
    void serialize_value(Qss::Context<D> *ctx, const T &value, D *target, typename boost::enable_if<is_metatype_defined<T> >::type * = NULL) {
        Qss::Serializer<D> *serializer = ctx->serializer(qMetaTypeId<T>());
        if(serializer) {
            serializer->serialize(ctx, static_cast<const void *>(&value), target);
        } else {
            serialize_value_direct(ctx, value, target);
        }
    }

    template<class T, class D>
    void serialize_value(Qss::Context<D> *ctx, const T &value, D *target, typename boost::disable_if<is_metatype_defined<T> >::type * = NULL) {
        serialize_value_direct(ctx, value, target);
    }

    template<class T, class D>
    bool deserialize_value(Qss::Context<D> *ctx, const D &value, T *target, typename boost::enable_if<is_metatype_defined<T> >::type * = NULL) {
        Qss::Serializer<D> *serializer = ctx->serializer(qMetaTypeId<T>());
        if(serializer) {
            return serializer->deserialize(ctx, value, static_cast<void *>(target));
        } else {
            return deserialize_value_direct(ctx, value, target);
        }
    }

    template<class T, class D>
    bool deserialize_value(Qss::Context<D> *ctx, const D &value, T *target, typename boost::disable_if<is_metatype_defined<T> >::type * = NULL) {
        return deserialize_value_direct(ctx, value, target);
    }

} // namespace QssDetail

namespace Qss {

    /* Public interface for (de)serializers that do not use context. */

    template<class T, class D>
    void serialize(const T &value, D *target) {
        assert(target);
        QssDetail::serialize_value(value, target);
    }

    template<class T, class D>
    bool deserialize(const D &value, T *target) {
        assert(target);
        return QssDetail::deserialize_value(value, target);
    }

    
    /* Public interface for (de)serializers that use context. */

    // TODO: use template for context, with static_assert, so that the 
    // actual type is passed down the call tree.

    template<class T, class D>
    void serialize(Context<D> *ctx, const T &value, D *target) {
        assert(ctx && target);
        QssDetail::serialize_value(ctx, value, target);
    }

    template<class T, class D>
    bool deserialize(Context<D> *ctx, const D &value, T *target) {
        assert(ctx && target);
        return QssDetail::deserialize_value(ctx, value, target);
    }


} // namespace Qss

#endif // QN_SERIALIZATION_H
