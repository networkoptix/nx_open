/* This file was derived from ArXLib with permission of the copyright holder.
 * See http://code.google.com/p/arxlib/. */
#ifndef QN_COLLECTION_H
#define QN_COLLECTION_H

#include <type_traits> /* For std::true_type, std::false_type. */

#ifndef Q_MOC_RUN
#include <boost/iterator/iterator_adaptor.hpp>
#include <boost/range/mutable_iterator.hpp>
#include <boost/range/const_iterator.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#endif

template<class T> class QList;
template<class Key, class T> class QHash;
template<class Key, class T> class QMultiHash;
template<class Key, class T> class QMap;
template<class Key, class T> class QMultiMap;
template<class T> class QSet;
class QJsonObject;


namespace QnCollection {
    struct list_tag {};
    struct set_tag {};
    struct map_tag {};
} // namespace QnCollection


namespace QnCollectionDetail {
    template<class Collection>
    std::true_type has_reserve_test(const Collection &, const decltype(&Collection::reserve) * = NULL);
    std::false_type has_reserve_test(...);

    template<class Collection>
    struct has_reserve {
        typedef decltype(has_reserve_test(std::declval<Collection>())) type;
    };

    template<class Collection>
    std::true_type has_key_type_test(const Collection &, const typename Collection::key_type * = NULL);
    std::false_type has_key_type_test(...);

    template<class Collection>
    struct has_key_type {
        typedef decltype(has_key_type_test(std::declval<Collection>())) type;
    };

    template<class Collection>
    std::true_type has_mapped_type_test(const Collection &, const typename Collection::mapped_type * = NULL);
    std::false_type has_mapped_type_test(...);

    template<class Collection>
    struct has_mapped_type {
        typedef decltype(has_mapped_type_test(std::declval<Collection>())) type;
    };

    template<class Collection, bool hasKeyType = has_key_type<Collection>::type::value, bool hasMappedType = has_mapped_type<Collection>::type::value>
    struct collection_category;

    template<class Collection>
    struct collection_category<Collection, false, false> {
        typedef QnCollection::list_tag type;
    };

    template<class Collection>
    struct collection_category<Collection, true, false> {
        typedef QnCollection::set_tag type;
    };

    template<class Collection>
    struct collection_category<Collection, true, true> {
        typedef QnCollection::map_tag type;
    };

    template<class Collection>
    void reserve(Collection &collection, int size, const std::true_type &) {
        collection.reserve(size);
    }

    template<class Collection>
    void reserve(Collection &, int, const std::false_type &) {
        return;
    }

} // namespace QnCollectionDetail


namespace QnCollection {

    template<class Collection, class Iterator, class Element>
    typename Collection::iterator insert(Collection &collection, const Iterator &pos, Element &&element) {
        return collection.insert(pos, std::forward<Element>(element));
    }

    /**
     * Qt containers are notorious for their lack of a unified insertion
     * operation.
     *
     * This problem is solved by introducing proper bindings.
     *
     * Note that QList, QVector and QLinkedList support STL-style insertion out of the box.
     */

    template<class T, class Iterator, class Element>
    typename QSet<T>::iterator insert(QSet<T> &collection, const Iterator &, Element &&element) {
        return collection.insert(std::forward<Element>(element));
    }

#define QN_REGISTER_QT_COLLECTION_INSERT(COLLECTION)                            \
    template<class Key, class T, class Iterator, class Element>                 \
    void insert(COLLECTION<Key, T> &collection, const Iterator &, Element &&element) { \
        collection.insert(element.first, element.second);                        \
    }

    QN_REGISTER_QT_COLLECTION_INSERT(QHash);
    QN_REGISTER_QT_COLLECTION_INSERT(QMultiHash);
    QN_REGISTER_QT_COLLECTION_INSERT(QMap);
    QN_REGISTER_QT_COLLECTION_INSERT(QMultiMap);
#undef QN_REGISTER_QT_COLLECTION_INSERT


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


    template<class Collection>
    void reserve(Collection &collection, int size) {
        QnCollectionDetail::reserve(collection, size, typename QnCollectionDetail::has_reserve<Collection>::type());
    }


    template<class Collection>
    void clear(Collection &collection) {
        collection.clear();
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


    template<class Collection>
    struct collection_category:
        QnCollectionDetail::collection_category<Collection>
    {};


    template<class T>
    struct make_assignable:
        std::remove_cv<typename std::remove_reference<T>::type>
    {};

    template<class T1, class T2>
    struct make_assignable<std::pair<T1, T2> > {
        typedef std::pair<
            typename make_assignable<T1>::type,
            typename make_assignable<T2>::type
        > type;
    };

} // namespace QnCollection


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


#define QN_REGISTER_QT_STL_MAP_ITERATOR(COLLECTION)                             \
namespace boost {                                                               \
    template<class Key, class T>                                                \
    struct range_mutable_iterator<COLLECTION<Key, T> > {                        \
        typedef QnStlMapIterator<typename COLLECTION<Key, T>::iterator, std::pair<const Key, T>, std::pair<const Key &, T &> > type; \
    };                                                                          \
                                                                                \
    template<class Key, class T>                                                \
    struct range_const_iterator<COLLECTION<Key, T> > {                          \
        typedef QnStlMapIterator<typename COLLECTION<Key, T>::const_iterator, std::pair<const Key, T>, std::pair<const Key &, const T &> > type; \
    };                                                                          \
} /* namespace boost */                                                         \
                                                                                \
template<class Key, class T>                                                    \
inline typename boost::range_mutable_iterator<COLLECTION<Key, T> >::type        \
range_begin(COLLECTION<Key, T> &x) {                                            \
    return typename boost::range_mutable_iterator<COLLECTION<Key, T> >::type(x.begin()); \
}                                                                               \
                                                                                \
template<class Key, class T>                                                    \
inline typename boost::range_const_iterator<COLLECTION<Key, T> >::type          \
range_begin(const COLLECTION<Key, T> &x) {                                      \
    return typename boost::range_const_iterator<COLLECTION<Key, T> >::type(x.begin()); \
}                                                                               \
                                                                                \
template<class Key, class T>                                                    \
inline typename boost::range_mutable_iterator<COLLECTION<Key, T> >::type        \
range_end(COLLECTION<Key, T> &x) {                                              \
    return typename boost::range_mutable_iterator<COLLECTION<Key, T> >::type(x.end()); \
}                                                                               \
                                                                                \
template<class Key, class T>                                                    \
inline typename boost::range_const_iterator<COLLECTION<Key, T> >::type          \
range_end(const COLLECTION<Key, T> &x) {                                        \
    return typename boost::range_const_iterator<COLLECTION<Key, T> >::type(x.end()); \
}

QN_REGISTER_QT_STL_MAP_ITERATOR(QHash);
QN_REGISTER_QT_STL_MAP_ITERATOR(QMultiHash);
QN_REGISTER_QT_STL_MAP_ITERATOR(QMap);
QN_REGISTER_QT_STL_MAP_ITERATOR(QMultiMap);
#undef QN_REGISTER_QT_STL_MAP_ITERATOR

// TODO: #Elric remove
/* Compatibility layer. */

template<class List>
void qnResizeList(List &list, int size) {
    QnCollection::resize(list, size);
}

#endif // QN_COLLECTION_H
