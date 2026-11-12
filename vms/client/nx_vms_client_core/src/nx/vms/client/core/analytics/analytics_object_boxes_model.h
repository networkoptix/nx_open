// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QAbstractListModel>
#include <QtCore/QRectF>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/scoped_model_operations.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/core/media/analytics_fwd.h>

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::analytics::db { class AbstractObjectTypeDictionary; }

namespace nx::vms::client::core {

namespace analytics { class AbstractObjectAppearanceProvider; }

/**
 * Analytics objects visible in the displayed video frame of a single channel. Rows are kept per
 * object track, so a delegate stays the same while its object moves.
 */
class NX_VMS_CLIENT_CORE_API AnalyticsObjectBoxesModel:
    public ScopedModelOperations<QAbstractListModel>
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::core::AbstractAnalyticsMetadataProviderPtr metadataProvider READ
            metadataProvider WRITE setMetadataProvider NOTIFY metadataProviderChanged)

    /** Timestamp of the displayed video frame, in milliseconds since epoch. */
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)

    /** Resource of the video, used to get object type names from its System. */
    Q_PROPERTY(QnResource* resource READ rawResource WRITE setRawResource NOTIFY resourceChanged)

    Q_PROPERTY(int channel READ channel WRITE setChannel NOTIFY channelChanged)

    /** Only objects intersecting this relative rectangle are shown. Not applied when invalid. */
    Q_PROPERTY(QRectF filterRect READ filterRect WRITE setFilterRect NOTIFY filterRectChanged)

    /** Text matched against object type names and attributes, as in the object search. */
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

    /** If not null, only the object of this track is shown. */
    Q_PROPERTY(nx::Uuid filterTrackId READ filterTrackId WRITE setFilterTrackId NOTIFY
            filterTrackIdChanged)

    using base_type = ScopedModelOperations<QAbstractListModel>;

public:
    enum Roles
    {
        TrackIdRole = core::ObjectTrackIdRole,
        ObjectTypeIdRole = core::ObjectTypeIdRole,
        TitleRole = Qt::DisplayRole,
        ColorRole = Qt::ForegroundRole,
        RectRole = core::PreviewHighlightRectRole,
        IsPointRole = core::CoreItemDataRoleCount,
    };
    Q_ENUM(Roles)

    explicit AnalyticsObjectBoxesModel(QObject* parent = nullptr);
    virtual ~AnalyticsObjectBoxesModel() override;

    AbstractAnalyticsMetadataProviderPtr metadataProvider() const;
    void setMetadataProvider(const AbstractAnalyticsMetadataProviderPtr& value);

    qint64 position() const;
    void setPosition(qint64 value);

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr& value);

    int channel() const;
    void setChannel(int value);

    QRectF filterRect() const;
    void setFilterRect(const QRectF& value);

    QString filterText() const;
    void setFilterText(const QString& value);

    nx::Uuid filterTrackId() const;
    void setFilterTrackId(const nx::Uuid& value);

    /**
     * Fetches the metadata at the current position again. To be called when new metadata is
     * received, as it is otherwise fetched only on position changes.
     */
    Q_INVOKABLE void refresh();

    virtual int rowCount(const QModelIndex& parent = {}) const override;
    virtual QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    static void registerQmlType();

signals:
    void metadataProviderChanged();
    void positionChanged();
    void resourceChanged();
    void channelChanged();
    void filterRectChanged();
    void filterTextChanged();
    void filterTrackIdChanged();

protected:
    /**
     * Replaces the default provider, which uses ObjectDisplaySettings and the taxonomy of the
     * resource System. Null restores the default one.
     */
    void setAppearanceProvider(std::shared_ptr<analytics::AbstractObjectAppearanceProvider> value);

    /**
     * Replaces the default dictionary, the taxonomy of the resource System, which the filter text
     * is matched against. Null restores the default one.
     */
    void setObjectTypeDictionary(
        std::shared_ptr<nx::analytics::db::AbstractObjectTypeDictionary> value);

private:
    QnResource* rawResource() const;
    void setRawResource(QnResource* value);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
