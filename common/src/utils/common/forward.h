#ifndef QN_FORWARD_H
#define QN_FORWARD_H

/**
 * A simple constructor forwarding macro that saves a lot of typing for
 * non-C++11 compilers.
 * 
 * \param CLASS                         Name of the enclosing class.
 * \param BASE                          Name of the base class to forward
 *                                      constructor arguments to.
 * \param ...                           Constructor body. The simpler, the better,
 *                                      as it will be copied multiple times.
 */
#define QN_FORWARD_CONSTRUCTOR(CLASS, BASE, ... /* BODY */)                     \
    CLASS(): BASE() __VA_ARGS__                                                 \
                                                                                \
    template<class T0>                                                          \
    CLASS(const T0 &arg0): BASE(arg0) __VA_ARGS__                               \
    template<class T0>                                                          \
    CLASS(T0 &arg0): BASE(arg0) __VA_ARGS__                                     \
                                                                                \
    template<class T0, class T1>                                                \
    CLASS(const T0 &arg0, const T1 &arg1): BASE(arg0, arg1) __VA_ARGS__         \
    template<class T0, class T1>                                                \
    CLASS(T0 &arg0, const T1 &arg1): BASE(arg0, arg1) __VA_ARGS__               \
    template<class T0, class T1>                                                \
    CLASS(const T0 &arg0, T1 &arg1): BASE(arg0, arg1) __VA_ARGS__               \
    template<class T0, class T1>                                                \
    CLASS(T0 &arg0, T1 &arg1): BASE(arg0, arg1) __VA_ARGS__                     \


#endif // QN_FORWARD_H
