#ifndef QN_CONTAINER_H
#define QN_CONTAINER_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QVector>

template<class T>
void qnResizeList(QList<T> &list, int size) {
    while(list.size() < size)
        list.push_back(T());
    while(list.size() > size)
        list.pop_back();
}

template<class T>
void qnResizeList(QVector<T> &list, int size) {
    list.resize(size);
}

template<class Key, class T>
void qnRemoveValues(QHash<Key, T> &hash, const T &value) {
    for(typename QHash<Key, T>::iterator pos = hash.begin(); pos != hash.end();) {
        if(pos.value() == value) {
            pos = hash.erase(pos);
        } else {
            pos++;
        }
    }
}

template<class List, class Pred>
int qnIndexOf(const List &list, int from, const Pred &pred) {
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
int qnIndexOf(const List &list, const Pred &pred) {
    return qnIndexOf(list, 0, pred);
}


#endif // QN_CONTAINER
