#ifndef QN_FLAT_MAP_H
#define QN_FLAT_MAP_H

#include <cassert>

#include <vector>
#include <type_traits> /* For std::is_empty and std::is_unsigned. */

#ifndef Q_MOC_RUN
#include <boost/functional/value_factory.hpp>
#endif

namespace QnFlatMapDetail {

    /* FactoryData implements empty base optimization. */

    template<class Factory, bool isEmpty = std::is_empty<Factory>::value> 
    class FactoryData {
    public:
        FactoryData(const Factory &factory): m_factory(factory) {}

        const Factory &factory() const { return m_factory; }

    private:
        Factory m_factory;
    };

    template<class Factory>
    class FactoryData<Factory, true> {
    public:
        FactoryData(const Factory &) {}

        Factory factory() const { return Factory(); }
    };

    template<class Container, class Key, class Factory>
    inline typename Container::value_type safe_value(const Container &list, const Key &key, const Factory &factory) {
        assert(key >= 0);

        if(key >= static_cast<Key>(list.size()))
            return factory();

        return list[key];
    }

    template<class Container, class Key, class Factory>
    inline typename Container::value_type &safe_reference(Container &list, const Key &key, const Factory &factory) {
        assert(key >= 0);

        while(key >= static_cast<Key>(list.size()))
            list.push_back(factory());

        return list[key];
    }

    template<class Key, class T, class Factory, class Container, bool isUnsigned = std::is_unsigned<Key>::value>
    class Data: public FactoryData<Factory> {
        typedef FactoryData<Factory> base_type;
    public:
        Data(const Factory &factory): base_type(factory) {}

        using base_type::factory;

        void clear() {
            m_positive.clear();
            m_negative.clear();
        }

        bool empty() const {
            return m_positive.empty() && m_negative.empty();
        }

        T value(const Key &key) const {
            if(key >= 0) {
                return safe_value(m_positive, key, factory());
            } else {
                return safe_value(m_negative, ~key, factory());
            }
        }

        T &reference(const Key &key) {
            if(key >= 0) {
                return safe_reference(m_positive, key, factory());
            } else {
                return safe_reference(m_negative, ~key, factory());
            }
        }

    private:
        Container m_positive, m_negative;
    };

    template<class Key, class T, class Factory, class Container>
    class Data<Key, T, Factory, Container, true>: public FactoryData<Factory> {
        typedef FactoryData<Factory> base_type;
    public:
        Data(const Factory &factory): base_type(factory) {}

        using base_type::factory;

        void clear() {
            m_positive.clear();
        }

        bool empty() const {
            return m_positive.empty();
        }

        T value(const Key &key) const {
            return safe_value(m_positive, key, factory());
        }

        T &reference(const Key &key) {
            return safe_reference(m_positive, key, factory());
        }

    private:
        Container m_positive;
    };

} // namespace QnFlatMapDetail


/**
 * Map with an integer key that uses arrays internally.
 */
template<class Key, class T, class DefaultValueFactory = boost::value_factory<T>, class Container = std::vector<T> >
class QnFlatMap {
public:
    QnFlatMap(const DefaultValueFactory &factory = DefaultValueFactory()): d(factory) {}

    /* Default copy constructor and assignment are OK. */

    void clear() {
        d.clear();
    }

    bool empty() const {
        return d.empty();
    }

    T value(const Key &key) const {
        return d.value(key);
    }

    void insert(const Key &key, const T &value) {
        d.reference(key) = value;
    }

    void remove(const Key &key) {
        d.reference(key) = d.factory()();
    }

    T operator[](const Key &key) const {
        return d.value(key);
    }

    T &operator[](const Key &key) {
        return d.reference(key);
    }

private:
    QnFlatMapDetail::Data<Key, T, DefaultValueFactory, Container> d;
};

#endif // QN_FLAT_MAP_H
