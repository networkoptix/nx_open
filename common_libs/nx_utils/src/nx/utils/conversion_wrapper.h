#ifndef QN_CONVERSION_WRAPPER_H
#define QN_CONVERSION_WRAPPER_H

#include <type_traits>

/**
 * A wrapper class that can be used to wrap values before invoking an 
 * overloaded function.
 * 
 * It adds global namespace into ADL namespace lookup list and prevents
 * user-defined type conversions from kicking in when picking the function 
 * to invoke.
 */
template<class T>
class QnConversionWrapper {
public:
    template<class E>
    QnConversionWrapper(const E &value, typename std::enable_if<std::is_same<T, E>::value>::type * = NULL): m_value(value) {}

    operator const T &() const {
        return m_value;
    }

private:
    const T &m_value;
};


typedef QnConversionWrapper<bool>               only_bool;
typedef QnConversionWrapper<char>               only_char;
typedef QnConversionWrapper<unsigned char>      only_unsigned_char;
typedef QnConversionWrapper<signed char>        only_signed_char;
typedef QnConversionWrapper<short>              only_short;
typedef QnConversionWrapper<unsigned short>     only_unsigned_short;
typedef QnConversionWrapper<int>                only_int;
typedef QnConversionWrapper<unsigned int>       only_unsigned_int;
typedef QnConversionWrapper<long>               only_long;
typedef QnConversionWrapper<unsigned long>      only_unsigned_long;
typedef QnConversionWrapper<long long>          only_long_long;
typedef QnConversionWrapper<unsigned long long> only_unsigned_long_long;
typedef QnConversionWrapper<float>              only_float;
typedef QnConversionWrapper<double>             only_double;


template<class T>
QnConversionWrapper<T> disable_user_conversions(const T &value) {
    return QnConversionWrapper<T>(value);
}



#endif // QN_CONVERSION_WRAPPER_H
