#pragma once

#include <QtCore/QHash>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QMimeData>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {

class MimeData
{
public:
    MimeData();
    explicit MimeData(const QMimeData* data);

    static QStringList mimeTypes();
    void toMimeData(QMimeData* data) const;

    QByteArray data(const QString& mimeType) const;
    void setData(const QString& mimeType, const QByteArray& data);

    QStringList formats() const;
    bool hasFormat(const QString& mimeType) const;
    void removeFormat(const QString& mimeType);

    void setResources(const QnResourceList& resources);

    QList<QnUuid> getIds() const;
    void setIds(const QList<QnUuid>& ids);

    QList<QUrl> getUrls() const;

    friend QDataStream& operator<<(QDataStream& stream, const MimeData& data);
    friend QDataStream &operator>>(QDataStream& stream, MimeData& data);

    QString serialized() const;

private:
    void load(const QMimeData* data);

private:
    QHash<QString, QByteArray> m_data;
};

} // namespace desktop
} // namespace client
} // namespace nx
