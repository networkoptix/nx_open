#ifndef QN_MPL_H
#define QN_MPL_H

template<class T>
struct remove_pointer {
    typedef T type;
};

template<class T>
struct remove_pointer<T *> {
    typedef T type;
};

template<class T>
struct remove_pointer<T *const> {
    typedef T type;
};

#endif // QN_MPL_H
