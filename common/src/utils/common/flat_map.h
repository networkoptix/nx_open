#ifndef QN_FLAT_MAP_H
#define QN_FLAT_MAP_H

#include <cassert>

#include <QtCore/QVector>

template<class T>
struct QnDefaultConstructor {
    T operator()() const {
        return T();
    }
};

template<class T, T value>
struct QnValueConstructor {
    T operator()() const {
        return value;
    }
};


/**
 * Map with an integer key that uses arrays internally.
 */
template<class Key, class T, class DefaultConstructor = QnDefaultConstructor<T>, class Container = QVector<T> >
class QnFlatMap {
public:
    QnFlatMap() {}

    /* Default copy constructor and assignment are OK. */

    void clear() {
        m_positive.clear();
        m_negative.clear();
    }

    bool empty() const {
        return m_positive.empty() && m_negative.empty();
    }

    T value(const Key &key) {
        if(key >= 0) {
            return valueInternal(m_positive, key);
        } else {
            return valueInternal(m_negative, ~key);
        }
    }

    void insert(const Key &key, const T &value) {
        operator[](key) = value;
    }

    T operator[](const Key &key) const {
        return value(key);
    }

    T &operator[](const Key &key) {
        if(key >= 0) {
            return referenceInternal(m_positive, key);
        } else {
            return referenceInternal(m_negative, ~key);
        }
    }

private:
    T valueInternal(const Container &list, const Key &key) const {
        assert(key >= 0);

        if(key >= list.size())
            return DefaultConstructor()();

        return list[key];
    }

    T &referenceInternal(Container &list, const Key &key) {
        assert(key >= 0);

        while(key >= list.size())
            list.push_back(DefaultConstructor()());

        return list[key];
    }

private:
    Container m_positive, m_negative;
};

#endif // QN_FLAT_MAP_H
