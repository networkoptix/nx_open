#ifndef QN_SHADER_SOURCE_H
#define QN_SHADER_SOURCE_H

#include <boost/preprocessor/stringize.hpp>

#include <QtCore/QByteArray>

inline QByteArray qnUnparenthesize(const char *parenthesized) {
    QByteArray result(parenthesized);
    return result.mid(1, result.size() - 2);
    return result;
}

#define QN_SHADER_SOURCE(...) qnUnparenthesize(BOOST_PP_STRINGIZE((__VA_ARGS__)))

#endif // QN_SHADER_SOURCE_H
