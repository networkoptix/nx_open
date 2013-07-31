#ifndef QN_CONTAINER_H
#define QN_CONTAINER_H

template<class T>
void resizeList(T &list, int size) {
    while(list.size() < size)
        list.push_back(typename T::value_type());
    while(list.size() > size)
        list.pop_back();
}

#endif // QN_CONTAINER
