/* This file was derived from ArXLib with permission of the copyright holder. 
 * See http://code.google.com/p/arxlib/. */
#ifndef QN_CONTAINER_H
#define QN_CONTAINER_H

#include <type_traits> /* For std::true_type, std::false_type. */

#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/range/mutable_iterator.hpp>
#include <boost/range/const_iterator.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>

template<class T> class QList;
template<class Key, class T> class QHash;
template<class Key, class T> class QMultiHash;
template<class Key, class T> class QMap;
template<class Key, class T> class QMultiMap;
template<class T> class QSet;


namespace QnContainerDetail {
    template<class Container>
    std::true_type has_reserve_test(const Container &, const decltype(&Container::reserve) * = NULL);
    std::false_type has_reserve_test(...);

    template<class Container>
    struct has_reserve {
        typedef decltype(has_reserve_test(std::declval<Container>())) type;
    };

    template<class Container>
    void reserve(Container &container, int size, const std::true_type &) {
        container.reserve(size);
    }

    template<class Container>
    void reserve(Container &, int, const std::false_type &) {
        return;
    }

} // namespace QnContainerDetail

namespace QnContainer {

    template<class Container, class Iterator, class Element>
    void insert(Container &container, const Iterator &pos, const Element &element) {
        container.insert(pos, element);
    }

    /**
     * Qt containers are also notorious for their lack of a unified insertion
     * operation.
     *
     * This problem is solved by introducing proper bindings.
     * 
     * Note that QList, QVector and QLinkedList support STL-style insertion out of the box. 
     */

    template<class T, class Iterator, class Element>
    void insert(QSet<T> &container, const Iterator &, const Element &element) {
        container.insert(element);
    }

#define QN_REGISTER_QT_CONTAINER_INSERT(CONTAINER)                              \
    template<class Key, class T, class Iterator, class Element>                 \
    void insert(CONTAINER<Key, T> &container, const Iterator &, const Element &element) { \
        container.insert(element.first, element.second);                        \
    }

    QN_REGISTER_QT_CONTAINER_INSERT(QHash);
    QN_REGISTER_QT_CONTAINER_INSERT(QMultiHash);
    QN_REGISTER_QT_CONTAINER_INSERT(QMap);
    QN_REGISTER_QT_CONTAINER_INSERT(QMultiMap);
#undef QN_REGISTER_QT_CONTAINER_INSERT


    template<class List>
    void resize(List &list, int size) {
        list.resize(size);
    }

    template<class T>
    void resize(QList<T> &list, int size) {
        while(list.size() < size)
            list.push_back(T());
        while(list.size() > size)
            list.pop_back();
    }


    template<class Container>
    void reserve(Container &container, int size) {
        QnContainerDetail::reserve(container, size, QnContainerDetail::has_reserve<Container>::type());
    }


    template<class Container>
    void clear(Container &container) {
        container.clear();
    }
    

    template<class List, class Pred>
    int indexOf(const List &list, int from, const Pred &pred) {
        if(from < 0)
            from = qMax(from + list.size(), 0);
        if(from < list.size()) {
            for(auto pos = list.begin(), end = list.end(); pos != end; pos++)
                if(pred(*pos))
                    return pos - list.begin();
        }
        return -1;
    }

    template<class List, class Pred>
    int indexOf(const List &list, const Pred &pred) {
        return indexOf(list, 0, pred);
    }

} // namespace QnContainer


/**
 * What this class does is it allows to iterate through Qt hash/map containers
 * using the same interface as with STL ones --- that is, iterator dereferencing
 * yields an <tt>std::pair</tt>.
 * 
 * This can be extremely useful when writing generic algorithms that are supposed
 * to work both with Qt and STL containers. The way to use this class is to
 * replace <tt>begin</tt> and <tt>end</tt> calls in your code with 
 * <tt>boost::begin</tt> and <tt>boost::end</tt>. It is that simple.
 */
template<class Iterator, class Value, class Reference>
class QnStlMapIterator: public
    boost::iterator_adaptor<
        QnStlMapIterator<Iterator, Value, Reference>,
        Iterator,
        Value,
        boost::bidirectional_traversal_tag,
        Reference
    >
{
public:
    QnStlMapIterator() {}

    explicit QnStlMapIterator(const Iterator &iterator):
        QnStlMapIterator::iterator_adaptor_(iterator) {}

    template<class OtherIterator, class OtherValue, class OtherReference>
    QnStlMapIterator(const QnStlMapIterator<OtherIterator, OtherValue, OtherReference> &other):
        QnStlMapIterator::iterator_adaptor_(other.base()) {}

private:
    friend class boost::iterator_core_access;

    Reference dereference() const {
        return Reference(QnStlMapIterator::base().key(), QnStlMapIterator::base().value());
    }
};


#define QN_REGISTER_QT_STL_MAP_ITERATOR(CONTAINER)                              \
namespace boost {                                                               \
    template<class Key, class T>                                                \
    struct range_mutable_iterator<CONTAINER<Key, T> > {                         \
        typedef QnStlMapIterator<typename CONTAINER<Key, T>::iterator, std::pair<Key, T>, std::pair<const Key &, T &> > type; \
    };                                                                          \
                                                                                \
    template<class Key, class T>                                                \
    struct range_const_iterator<CONTAINER<Key, T> > {                           \
        typedef QnStlMapIterator<typename CONTAINER<Key, T>::const_iterator, std::pair<Key, T>, std::pair<const Key &, const T &> > type; \
    };                                                                          \
} /* namespace boost */                                                         \
                                                                                \
template<class Key, class T>                                                    \
inline typename boost::range_mutable_iterator<CONTAINER<Key, T> >::type         \
range_begin(CONTAINER<Key, T> &x) {                                             \
    return typename boost::range_mutable_iterator<CONTAINER<Key, T> >::type(x.begin()); \
}                                                                               \
                                                                                \
template<class Key, class T>                                                    \
inline typename boost::range_const_iterator<CONTAINER<Key, T> >::type           \
range_begin(const CONTAINER<Key, T> &x) {                                       \
    return typename boost::range_const_iterator<CONTAINER<Key, T> >::type(x.begin()); \
}                                                                               \
                                                                                \
template<class Key, class T>                                                    \
inline typename boost::range_mutable_iterator<CONTAINER<Key, T> >::type         \
range_end(CONTAINER<Key, T> &x) {                                               \
    return typename boost::range_mutable_iterator<CONTAINER<Key, T> >::type(x.end()); \
}                                                                               \
                                                                                \
template<class Key, class T>                                                    \
inline typename boost::range_const_iterator<CONTAINER<Key, T> >::type           \
range_end(const CONTAINER<Key, T> &x) {                                         \
    return typename boost::range_const_iterator<CONTAINER<Key, T> >::type(x.end()); \
}

QN_REGISTER_QT_STL_MAP_ITERATOR(QT_PREPEND_NAMESPACE(QHash));
QN_REGISTER_QT_STL_MAP_ITERATOR(QT_PREPEND_NAMESPACE(QMultiHash));
QN_REGISTER_QT_STL_MAP_ITERATOR(QT_PREPEND_NAMESPACE(QMap));
QN_REGISTER_QT_STL_MAP_ITERATOR(QT_PREPEND_NAMESPACE(QMultiMap));
#undef QN_REGISTER_QT_STL_MAP_ITERATOR



// TODO: #Elric remove
/* Compatibility layer. */

template<class List>
void qnResizeList(List &list, int size) {
    QnContainer::resize(list, size);
}

template<class List, class Pred>
int qnIndexOf(const List &list, int from, const Pred &pred) {
    return QnContainer::indexOf(list, from, pred);
}

template<class List, class Pred>
int qnIndexOf(const List &list, const Pred &pred) {
    return QnContainer::indexOf(list, pred);
}

#endif // QN_CONTAINER
