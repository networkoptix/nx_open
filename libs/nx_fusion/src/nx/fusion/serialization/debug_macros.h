#pragma once

#include <type_traits>

#include <QtCore/QDebug>
#include <QtCore/QFlags>
#include <QtCore/QString>

#include <nx/fusion/fusion/fusion.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/lexical_enum.h>

namespace QnDebugSerialization {

class SerializationVisitor
{
public:
    SerializationVisitor(QDebug &stream):
        m_stream(stream)
    {
    }

    template<class T, class Access>
    bool operator()(const T&, const Access& access, const QnFusion::start_tag&)
    {
        using namespace QnFusion;
        m_stream.nospace() << access(c_classname) << " {";
        return true;
    }

    template<class T, class Access>
    bool operator()(const T& value, const Access& access) {
        using namespace QnFusion;
        m_stream.nospace() << access(c_name) << ": " << invoke(access(getter), value) << "; ";
        return true;
    }

    template<class T, class Access>
    bool operator()(const T &, const Access &, const QnFusion::end_tag &) {
        m_stream.nospace() << '}';
        m_stream.space();
        return true;
    }

private:
    QDebug &m_stream;
};

template<typename T>
struct DefaultDebugSerializer
{
    QDebug& operator()(QDebug& stream, const T& value)
    {
        return (stream << value);
    }
};

template<typename T>
struct EnumDebugSerializer
{
    QDebug& operator()(QDebug& stream, const T& value)
    {
        return (stream << QnLexical::serialized(value));
    }
};

template<typename T>
struct ClassDebugSerializer
{
    QDebug& operator()(QDebug& stream, const T& value)
    {
        QnFusion::visit_members(value, QnDebugSerialization::SerializationVisitor(stream));
        return stream;
    }
};

template<typename T>
struct ClassDebugSerializer<QFlags<T>>
{
    QDebug& operator()(QDebug& stream, const QFlags<T>& value)
    {
        return (stream << QnLexical::serialized(value));
    }
};

template<typename T>
struct DebugSerializer: public std::conditional<
    std::is_class<T>::value,
        ClassDebugSerializer<T>,
        typename std::conditional<std::is_enum<T>::value,
            EnumDebugSerializer<T>,
            DefaultDebugSerializer<T>>::type>::type
{
};

} // namespace QnDebugSerialization


/**
 * This macro generates <tt>QDebug</tt> stream functions for
 * the given struct type.
 *
 * \param TYPE                          Struct type to define <tt>QDebug</tt> stream functions for.
 * \param FIELD_SEQ                     Preprocessor sequence of all fields of the
 *                                      given type that are to be streamed.
 * \param PREFIX                        Optional function definition prefix, e.g. <tt>inline</tt>.
 */
#define QN_FUSION_DEFINE_FUNCTIONS_debug(TYPE, ... /* PREFIX */)         \
__VA_ARGS__ QDebug& operator<<(QDebug& stream, const TYPE& value)        \
{                                                                        \
    return QnDebugSerialization::DebugSerializer<TYPE>()(stream, value); \
}                                                                        \
                                                                         \
__VA_ARGS__ void PrintTo(const TYPE& value, ::std::ostream* os)          \
{                                                                        \
    QString content;                                                     \
    QDebug dbg(&content);                                                \
    QnDebugSerialization::DebugSerializer<TYPE>()(dbg, value);           \
    *os << content.toStdString();                                        \
}                                                                        \
