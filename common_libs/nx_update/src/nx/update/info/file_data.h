#pragma once

#include <QtCore>

namespace nx {
namespace update {
namespace info {

struct FileData
{
    QString file;
    QString url;
    qint64 size;
    QByteArray md5;

    FileData() = default;
    FileData(const QString& file, const QString& url, qint64 size, const QByteArray& md5):
        file(file),
        url(url),
        size(size),
        md5(md5)
    {}

    FileData(FileData&&) = default;
    FileData(const FileData&) = default;
    FileData& operator = (FileData&&) = default;
    FileData& operator = (const FileData&) = default;
};

inline bool operator == (const FileData& lhs, const FileData& rhs)
{
    return lhs.file == rhs.file && lhs.url == rhs.url && lhs.size == rhs.size && lhs.md5 == rhs.md5;
}

} // namespace info
} // namespace update
} // namespace nx
