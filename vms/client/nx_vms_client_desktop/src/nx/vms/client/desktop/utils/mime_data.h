// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QMimeData>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class WindowContext;

/**
 * Utility class to handle in-app and between-app drag-n-drop. Stores list of dragged resources
 * and other entities (e.g. Showreels or videowall items).
 * This class can convert to and from QMimeData to support standard mechanisms. During conversion
 * all resources and entities are placed into two lists: list of ids and an additional list of
 * urls to handle local files drag-n-drop.
 */
class NX_VMS_CLIENT_DESKTOP_API MimeData
{
public:
    MimeData();
    MimeData(const QMimeData* data);
    MimeData(
        QByteArray serializedData,
        std::function<QnResourcePtr(nx::vms::common::ResourceDescriptor)>
            createResourceCallback = {});
    ~MimeData();

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
     * Get list of dragged entities (not resources), e.g. Showreels or videowall items.
     */
    QList<QnUuid> entities() const;
    void setEntities(const QList<QnUuid>& ids);

    QHash<int, QVariant> arguments() const;
    void setArguments(const QHash<int, QVariant>& value);

    /** Add user id and cloud system id (if the user is cloud). */
    void addContextInformation(const WindowContext* context);

    /**
     * Check whether the give mime data has user id and cloud system id and the values are equal
     * to the values in the given context.
     */
    bool allowedInWindowContext(const WindowContext* context) const;

    QString serialized() const;

    bool isEmpty() const;

private:
    void load(
        const QMimeData* data,
        std::function<QnResourcePtr(nx::vms::common::ResourceDescriptor)>
            createResourceCallback = {});

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
