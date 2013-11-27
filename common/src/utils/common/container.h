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


#endif // QN_CONTAINER
