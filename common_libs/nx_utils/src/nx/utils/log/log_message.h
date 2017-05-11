#pragma once

#include <string>
#include <type_traits>

#include <QtCore/QByteArray>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QSize>
#include <QtCore/QRect>

#include <nx/utils/log/to_string.h>
#include <nx/utils/uuid.h>

/**
 * Adds some useful overloads to QString::arg.
 */
class NX_UTILS_API QnLogMessage
{
public:
    static constexpr wchar_t kSpace = u' ';

    QnLogMessage();
    QnLogMessage(const char* text);
    QnLogMessage(const QString& text);
    QnLogMessage(const QByteArray& text);

    QnLogMessage arg(const QByteArray& a, int fieldWidth = 0, wchar_t fillChar = kSpace) const;
    QnLogMessage arg(const std::string& a, int fieldWidth = 0, wchar_t fillChar = kSpace) const;
    QnLogMessage arg(const char* str, int fieldWidth = 0, wchar_t fillChar = kSpace) const;
    QnLogMessage arg(const wchar_t* str, int fieldWidth = 0, wchar_t fillChar = kSpace) const;
    QnLogMessage arg(const QnUuid& a, int fieldWidth = 0, wchar_t fillChar = kSpace) const;
    QnLogMessage arg(const QUrl& a, int fieldWidth = 0, wchar_t fillChar = kSpace) const;

    // Support standard Qt UI types
    QnLogMessage arg(const QPoint& point) const;
    QnLogMessage arg(const QPointF& point) const;
    QnLogMessage arg(const QSize& size) const;
    QnLogMessage arg(const QSizeF& size) const;
    QnLogMessage arg(const QRect& rect) const;
    QnLogMessage arg(const QRectF& rect) const;


    QnLogMessage arg(std::chrono::seconds a, int fieldWidth = 0, wchar_t fillChar = kSpace) const;
    QnLogMessage arg(std::chrono::milliseconds a, int fieldWidth = 0, wchar_t fillChar = kSpace) const;
    QnLogMessage arg(std::chrono::microseconds a, int fieldWidth = 0, wchar_t fillChar = kSpace) const;

    template<typename T>
    QnLogMessage arg(T* a,int fieldWidth = 0, wchar_t fillChar = kSpace) const
    {
        return m_str.arg(QString::fromLatin1("0x%1").arg(
            reinterpret_cast<qulonglong>(a), fieldWidth - 2, 16, QChar(fillChar)));
    }

    template<typename T, typename ... Args>
    QnLogMessage arg(const std::unique_ptr<T>& a, const Args& ... args) const
    {
        return arg(a.get(), args ...);
    }

    template<typename T, typename ... Args>
    QnLogMessage arg(const std::shared_ptr<T>& a, const Args& ... args) const
    {
        return arg(a.get(), args ...);
    }

    template<typename T, typename ... Args>
    QnLogMessage arg(const boost::optional<T>& a, const Args& ... args) const
    {
        return arg(toString(a), args ...);
    }

    template<typename ... Args>
    QnLogMessage arg(const Args& ... args) const
    {
        return m_str.arg(args ...);
    }

    template<typename T, typename ... Args>
    QnLogMessage args(const T& a, const Args& ... args) const
    {
        return arg(a).args(args ...);
    }

    template<typename T>
    QnLogMessage args(const T& a) const
    {
        return arg(a);
    }

    template<typename ... Args>
    QnLogMessage str(const Args& ... args) const
    {
        return arg(toString(args ...));
    }

    template<typename ... Args>
    QnLogMessage strs(const Args& ... args) const
    {
        return arg(toString(args) ...);
    }

    template<typename ... Args>
    QnLogMessage container(const Args& ... args) const
    {
        return arg(containerString(args ...));
    }

    operator QString() const;
    std::string toStdString() const;

private:
    QString m_str;
};

typedef QnLogMessage lm;
