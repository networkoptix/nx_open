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

/**
 * Utility class to handle in-app and between-app drag-n-drop. Stores list of dragged resources
 * and other entities (e.g. layout tours or videowall items).
 * This class can convert to and from QMimeData to support standard mechanisms. During conversion
 * all resources and entities are placed into two lists: list of ids and an additional list of
 * urls to handle local files drag-n-drop.
 */
class MimeData
{
public:
    MimeData();
    MimeData(const QMimeData* data, QnResourcePool* resourcePool);

    static QStringList mimeTypes();
    void toMimeData(QMimeData* data) const;
    QMimeData* createMimeData() const;

    QByteArray data(const QString& mimeType) const;
    void setData(const QString& mimeType, const QByteArray& data);

    QStringList formats() const;
    bool hasFormat(const QString& mimeType) const;
    void removeFormat(const QString& mimeType);

    /**
     * Get list of dragged resources.
     * Note: some of them may not belong to the resource pool (e.g. local files).
     */
    QnResourceList resources() const;
    void setResources(const QnResourceList& resources);

    /**
     * Get list of dragged entities (not resources), e.g. layout tours or videowall items.
     */
    QList<QnUuid> entities() const;
    void setEntities(const QList<QnUuid>& ids);

    QString serialized() const;
    static MimeData deserialized(QByteArray data, QnResourcePool* resourcePool);

    bool isEmpty() const;

private:
    void load(const QMimeData* data, QnResourcePool* resourcePool);
    void updateInternalStorage();

private:
    QHash<QString, QByteArray> m_data;

    QList<QnUuid> m_entities;
    QnResourceList m_resources;
};

} // namespace desktop
} // namespace client
} // namespace nx
