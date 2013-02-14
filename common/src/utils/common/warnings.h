#ifndef QN_WARNINGS_H
#define QN_WARNINGS_H

#if defined(_MSC_VER) && _MSC_VER<1600 
// TODO: msvc2008, remove this hell after transition to msvc2010
#   ifdef _WIN64
namespace std { typedef __int64 intptr_t; }
#   else
namespace std { typedef __int32 intptr_t; }
#   endif
#else
#   include <cstdint> /* For std::intptr_t. */
#endif

#include <QtCore/QString>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>
#include <QtCore/QVariant>
#include <QtCore/QUrl>

#include "preprocessor.h"

namespace detail {
    inline void debugInternal(const char *functionName, const QString &s) {
        qDebug("%s: %s", functionName, qPrintable(s));
    }

    inline void warningInternal(const char *functionName, const QString &s) {
        qWarning("%s: %s", functionName, qPrintable(s));
    }

    inline void criticalInternal(const char *functionName, const QString &s) {
        qCritical("%s: %s", functionName, qPrintable(s));
    }

    inline void fatalInternal(const char *functionName, const QString &s) {
        qFatal("%s: %s", functionName, qPrintable(s));
    }

    typedef void (*WarningHandler)(const char *, const QString &);

    namespace operators {

        template<class T>
        QString operator<<(const QString &s, const T &arg) {
            return s.arg(arg);
        }

        inline QString operator<<(const QString &s, const QUrl &arg) {
            return s.arg(arg.toString());
        }

        inline QString operator<<(const QString &s, const char *arg) {
            return s.arg(QLatin1String(arg));
        }

        inline QString operator<<(const QString &s, const wchar_t *arg) {
            return s.arg(QString::fromWCharArray(arg));
        }

        inline QString operator<<(const QString &s, char *arg) {
            return s.arg(QLatin1String(arg));
        }

        inline QString operator<<(const QString &s, wchar_t *arg) {
            return s.arg(QString::fromWCharArray(arg));
        }

        inline QString operator<<(const QString &s, const QByteArray &arg) {
            return s.arg(QLatin1String(arg.data()));
        }

        template<class T>
        inline QString textstream_operator_lshift(const QString &s, const T &arg) {
            QString text;
            QTextStream stream(&text, QIODevice::WriteOnly);
            stream << arg;
            return s.arg(text);
        }

        inline QString operator<<(const QString &s, const void *arg) {
            return textstream_operator_lshift(s, arg);
        }

        template<class T>
        inline QString debug_operator_lshift(const QString &s, const T &arg) {
            QString text;
            QDebug stream(&text);
            stream << arg;
            return s.arg(text);
        }

        inline QString operator<<(const QString &s, const QObject *arg) {
            return debug_operator_lshift(s, arg);
        }

        template<class T>
        inline QString operator<<(const QString &s, const QList<T> &arg) {
            return debug_operator_lshift(s, arg);
        }

        template<class T>
        inline QString operator<<(const QString &s, const QVector<T> &arg) {
            return debug_operator_lshift(s, arg);
        }

        template<class Key, class T>
        inline QString operator<<(const QString &s, const QHash<Key, T> &arg) {
            return debug_operator_lshift(s, arg);
        }

        template<class T>
        inline QString operator<<(const QString &s, const QSet<T> &arg) {
            return debug_operator_lshift(s, arg);
        }

        inline QString operator<<(const QString &s, const QVariant &arg) {
            return debug_operator_lshift(s, arg);
        }

        inline void invokeInternal(const WarningHandler &handler, const char *functionName, const QString &s) {
            handler(functionName, s);
        }

        template<class T0>
        inline void invokeInternal(const WarningHandler &handler, const char *functionName, const QString &s, const T0 &arg0) {
            handler(functionName, s << arg0);
        }

        template<class T0, class T1>
        inline void invokeInternal(const WarningHandler &handler, const char *functionName, const QString &s, const T0 &arg0, const T1 &arg1) {
            handler(functionName, s << arg0 << arg1);
        }

        template<class T0, class T1, class T2>
        inline void invokeInternal(const WarningHandler &handler, const char *functionName, const QString &s, const T0 &arg0, const T1 &arg1, const T2 &arg2) {
            handler(functionName, s << arg0 << arg1 << arg2);
        }

        template<class T0, class T1, class T2, class T3>
        inline void invokeInternal(const WarningHandler &handler, const char *functionName, const QString &s, const T0 &arg0, const T1 &arg1, const T2 &arg2, const T3 &arg3) {
            handler(functionName, s << arg0 << arg1 << arg2 << arg3);
        }

        template<class T0, class T1, class T2, class T3, class T4>
        inline void invokeInternal(const WarningHandler &handler, const char *functionName, const QString &s, const T0 &arg0, const T1 &arg1, const T2 &arg2, const T3 &arg3, const T4 &arg4) {
            handler(functionName, s << arg0 << arg1 << arg2 << arg3 << arg4);
        }

    } // namespace operators


    template<class T>
    const char *nullName(const T *) {
        return "NULL";
    }

    template<class T>
    const char *nullName(const T &) {
        return "null";
    }

} // namespace detail



#define qnDebug(MSG, ...)                                                       \
    ::detail::operators::invokeInternal(&::detail::debugInternal,    Q_FUNC_INFO, QLatin1String(MSG), ##__VA_ARGS__)

#define qnWarning(MSG, ...)                                                     \
    ::detail::operators::invokeInternal(&::detail::warningInternal,  Q_FUNC_INFO, QLatin1String(MSG), ##__VA_ARGS__)

#define qnCritical(MSG, ...)                                                    \
    ::detail::operators::invokeInternal(&::detail::criticalInternal, Q_FUNC_INFO, QLatin1String(MSG), ##__VA_ARGS__)

#define qnFatal(MSG, ...)                                                       \
    ::detail::operators::invokeInternal(&::detail::fatalInternal,    Q_FUNC_INFO, QLatin1String(MSG), ##__VA_ARGS__)



#define QN_NULL_PARAMETER_I(MACRO, PARAMETER) {                                 \
    (void) (PARAMETER); /* Show compilation error if parameter name is mistyped. */ \
    MACRO("Unexpected %1 parameter '%2'.", ::detail::nullName(PARAMETER), QN_STRINGIZE(PARAMETER)); \
}

/**
 * Emits an "unexpected NULL parameter" warning.
 * 
 * \param PARAMETER                    Parameter that was found out to be NULL.
 */
#define qnNullWarning(PARAMETER) QN_NULL_PARAMETER_I(qnWarning, PARAMETER)

/**
 * Emits an "unexpected NULL parameter" critical message.
 * 
 * \param PARAMETER                    Parameter that was found out to be NULL.
 */
#define qnNullCritical(PARAMETER) QN_NULL_PARAMETER_I(qnCritical, PARAMETER)

#endif // QN_WARNINGS_H
