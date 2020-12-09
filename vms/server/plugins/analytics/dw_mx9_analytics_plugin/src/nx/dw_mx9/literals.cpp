#include "literals.h"

#include <QList>
#include <QStringList>

QByteArray linesTrimmed(const QByteArray& document)
{
#if 0
    QByteArray result;
    result.reserve(document.size());

    bool newLine = true;
    for (const char c : document)
    {
        if (newLine)
        {
            if (std::isspace(c))
                continue;
            else
                newLine = false;
        }

        result.push_back(c);

        if (c == '\n')
            newLine = true;
    }
    return result;
#endif
    QByteArray result;
    result.reserve(document.size());

    QList<QByteArray> lines = document.split('\n');
    for (auto& line : lines)
        line = line.trimmed();

    lines.removeAll({});

    auto it = lines.cbegin();
    if (it != lines.cend())
    {
        result += *it;
        ++it;
    }
    for (; it != lines.cend(); ++it)
    {
        result += '\n';
        result += *it;
    }
    return result;
}

QString linesTrimmed(const QString& document)
{
    QStringList lines = document.split('\n');
    for (auto& line : lines)
        line = line.trimmed();

    lines.removeAll({});

    return lines.join('\n');
}

QByteArray operator""_linesTrimmed(const char* data, size_t size)
{
    QByteArray xml(data, size);
    return linesTrimmed(xml);
}

QString operator""_linesTrimmed(const char16_t* data, size_t size)
{
    QString xml(reinterpret_cast<const QChar*>(data), size);
    return linesTrimmed(xml);
}
