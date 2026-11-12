// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_object_boxes_model.h"

#include <algorithm>
#include <optional>
#include <unordered_set>
#include <vector>

#include <QtQml/QtQml>

#include <analytics/db/analytics_db_types.h>
#include <core/resource/resource.h>
#include <nx/vms/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/utils/geometry.h>

#include "object_appearance_provider.h"
#include "object_tracker.h"

namespace nx::vms::client::core {

using namespace std::chrono;
using namespace nx::common::metadata;
using analytics::AbstractObjectAppearanceProvider;
using analytics::ObjectAppearanceProvider;
using analytics::ObjectSnapshot;
using analytics::ObjectTracker;

namespace {

static constexpr auto kShowAsPointAttributeName = "nx.sys.showAsPoint";

bool isPoint(const ObjectMetadata& object)
{
    return std::ranges::any_of(object.attributes,
        [](const Attribute& attribute) { return attribute.name == kShowAsPointAttributeName; });
}

bool isValidRectangle(const QRectF& rect)
{
    // Rectangle can be used as a point, so zero width and height are OK.
    return rect.width() >= 0 && rect.height() >= 0;
}

struct Row
{
    nx::Uuid trackId;
    QString objectTypeId;
    QRectF rect;
    bool isPoint = false;

    QColor color;
    QString title;

    bool operator==(const Row&) const = default;
};

} // namespace

struct AnalyticsObjectBoxesModel::Private
{
    AnalyticsObjectBoxesModel* const q;

    AbstractAnalyticsMetadataProviderPtr metadataProvider;
    milliseconds position{0};
    QnResourcePtr resource;
    int channel = 0;

    /** Holds the values of the filter properties. */
    nx::analytics::db::Filter filter;

    std::shared_ptr<AbstractObjectAppearanceProvider> customAppearanceProvider;
    std::shared_ptr<ObjectAppearanceProvider> defaultAppearanceProvider;
    std::shared_ptr<nx::analytics::db::AbstractObjectTypeDictionary> customObjectTypeDictionary;

    ObjectTracker tracker;
    std::vector<Row> rows;

    const nx::analytics::db::AbstractObjectTypeDictionary* objectTypeDictionary() const
    {
        return customObjectTypeDictionary ? customObjectTypeDictionary.get()
                                          : defaultAppearanceProvider->objectTypeDictionary();
    }

    AbstractObjectAppearanceProvider* appearanceProvider() const
    {
        return customAppearanceProvider ? customAppearanceProvider.get()
                                        : defaultAppearanceProvider.get();
    }

    void updateDefaultAppearanceProvider()
    {
        defaultAppearanceProvider = std::make_shared<ObjectAppearanceProvider>(
            resource ? SystemContext::fromResource(resource) : nullptr);
    }

    /** Forgets everything known about the objects. */
    void clear()
    {
        tracker.clear();

        if (!rows.empty())
        {
            const ScopedReset scopedReset(q);
            rows.clear();
        }
    }

    void update()
    {
        if (!metadataProvider)
        {
            clear();
            return;
        }

        const auto packets =
            metadataProvider->metadataRange(position - ObjectTracker::kMetadataWindowSize,
                position + ObjectTracker::kMetadataWindowSize,
                channel);

        tracker.update(packets, position);
        updateRows();
    }

    /**
     * Recalculates the rows from the tracked objects without fetching new metadata. The rows of
     * the remaining tracks keep their positions, and the rows of new tracks are appended.
     */
    void updateRows()
    {
        std::unordered_set<nx::Uuid> trackIdsToShow;
        for (const auto& object: tracker.objects())
        {
            if (shouldShow(object))
                trackIdsToShow.insert(object.trackId);
        }

        // The rows are walked backwards, so removing rows never shifts the rows yet to walk.
        int lastRowToRemove = -1;
        const auto removeRows = [this, &lastRowToRemove](int first)
        {
            if (lastRowToRemove < first)
                return;

            const ScopedRemoveRows scopedRemoveRows(q, first, lastRowToRemove);
            rows.erase(rows.begin() + first, rows.begin() + lastRowToRemove + 1);
            lastRowToRemove = -1;
        };

        for (int i = int(rows.size()) - 1; i >= 0; --i)
        {
            if (trackIdsToShow.erase(rows[i].trackId) == 0)
            {
                if (lastRowToRemove < 0)
                    lastRowToRemove = i;
                continue;
            }

            removeRows(i + 1);

            auto row = makeRow(*tracker.objects().constFind(rows[i].trackId));
            if (row == rows[i])
                continue;

            rows[i] = std::move(row);
            const auto index = q->index(i);
            emit q->dataChanged(index, index);
        }
        removeRows(0);

        // The remaining ones are the tracks without rows.
        if (trackIdsToShow.empty())
            return;

        const int first = int(rows.size());
        const ScopedInsertRows scopedInsertRows(q, first, first + int(trackIdsToShow.size()) - 1);
        for (const auto& trackId: trackIdsToShow)
            rows.push_back(makeRow(*tracker.objects().constFind(trackId)));
    }

    /** Whether the object should have a row: it passes the filter and has a valid rectangle. */
    bool shouldShow(const ObjectSnapshot& object) const
    {
        return isValidRectangle(object.interpolatedRectangle(position)) && accepts(object);
    }

    bool accepts(const ObjectSnapshot& object) const
    {
        return filter.acceptsMetadata(resource ? resource->getId() : nx::Uuid(),
            object.analyticsEngineId,
            object.metadata,
            *objectTypeDictionary());
    }

    Row makeRow(const ObjectSnapshot& object) const
    {
        static constexpr QRectF kFrameBounds(0.0, 0.0, 1.0, 1.0);
        const auto provider = appearanceProvider();

        return Row{.trackId = object.trackId,
            .objectTypeId = QString::fromStdString(object.metadata.typeId),
            .rect = Geometry::movedInto(object.interpolatedRectangle(position), kFrameBounds),
            .isPoint = isPoint(object.metadata),
            .color = provider->color(object.metadata),
            .title = provider->title(object.metadata)};
    }
};

AnalyticsObjectBoxesModel::AnalyticsObjectBoxesModel(QObject* parent):
    base_type(parent),
    d(new Private{.q = this})
{
    d->updateDefaultAppearanceProvider();
}

AnalyticsObjectBoxesModel::~AnalyticsObjectBoxesModel()
{
}

AbstractAnalyticsMetadataProviderPtr AnalyticsObjectBoxesModel::metadataProvider() const
{
    return d->metadataProvider;
}

void AnalyticsObjectBoxesModel::setMetadataProvider(
    const AbstractAnalyticsMetadataProviderPtr& value)
{
    if (d->metadataProvider == value)
        return;

    d->metadataProvider = value;
    d->clear();
    d->update();
    emit metadataProviderChanged();
}

qint64 AnalyticsObjectBoxesModel::position() const
{
    return d->position.count();
}

void AnalyticsObjectBoxesModel::setPosition(qint64 value)
{
    const milliseconds position(value);
    if (d->position == position)
        return;

    d->position = position;
    d->update();
    emit positionChanged();
}

QnResourcePtr AnalyticsObjectBoxesModel::resource() const
{
    return d->resource;
}

void AnalyticsObjectBoxesModel::setResource(const QnResourcePtr& value)
{
    if (d->resource == value)
        return;

    d->resource = value;
    d->updateDefaultAppearanceProvider();

    // The metadata provider may still have the metadata of the previous resource here. It is
    // replaced on the same resource change of the media player, before anything is rendered.
    d->clear();
    d->update();
    emit resourceChanged();
}

QnResource* AnalyticsObjectBoxesModel::rawResource() const
{
    return d->resource.data();
}

void AnalyticsObjectBoxesModel::setRawResource(QnResource* value)
{
    setResource(value ? value->toSharedPointer() : QnResourcePtr());
}

int AnalyticsObjectBoxesModel::channel() const
{
    return d->channel;
}

void AnalyticsObjectBoxesModel::setChannel(int value)
{
    if (d->channel == value)
        return;

    d->channel = value;
    d->clear();
    d->update();
    emit channelChanged();
}

QRectF AnalyticsObjectBoxesModel::filterRect() const
{
    return d->filter.boundingBox.value_or(QRectF());
}

void AnalyticsObjectBoxesModel::setFilterRect(const QRectF& value)
{
    const auto boundingBox = value.isValid() ? std::make_optional(value) : std::nullopt;
    if (d->filter.boundingBox == boundingBox)
        return;

    d->filter.boundingBox = boundingBox;
    d->updateRows();
    emit filterRectChanged();
}

QString AnalyticsObjectBoxesModel::filterText() const
{
    return d->filter.freeText;
}

void AnalyticsObjectBoxesModel::setFilterText(const QString& value)
{
    if (d->filter.freeText == value)
        return;

    d->filter.freeText = value;

    // The text is parsed once instead of for every object.
    d->filter.context = value.isEmpty()
        ? std::nullopt
        : std::make_optional<nx::analytics::db::Filter::AcceptTrackContext>(value);

    d->updateRows();
    emit filterTextChanged();
}

nx::Uuid AnalyticsObjectBoxesModel::filterTrackId() const
{
    return d->filter.objectTrackId;
}

void AnalyticsObjectBoxesModel::setFilterTrackId(const nx::Uuid& value)
{
    if (d->filter.objectTrackId == value)
        return;

    d->filter.objectTrackId = value;
    d->updateRows();
    emit filterTrackIdChanged();
}

void AnalyticsObjectBoxesModel::setAppearanceProvider(
    std::shared_ptr<AbstractObjectAppearanceProvider> value)
{
    d->customAppearanceProvider = std::move(value);
    d->clear();
    d->update();
}

void AnalyticsObjectBoxesModel::setObjectTypeDictionary(
    std::shared_ptr<nx::analytics::db::AbstractObjectTypeDictionary> value)
{
    d->customObjectTypeDictionary = std::move(value);
    d->updateRows();
}

void AnalyticsObjectBoxesModel::refresh()
{
    d->update();
}

int AnalyticsObjectBoxesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(d->rows.size());
}

QVariant AnalyticsObjectBoxesModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const auto& row = d->rows[index.row()];
    switch (role)
    {
        case TrackIdRole:
            return QVariant::fromValue(row.trackId);
        case ObjectTypeIdRole:
            return row.objectTypeId;
        case RectRole:
            return row.rect;
        case IsPointRole:
            return row.isPoint;
        case ColorRole:
            return row.color;
        case TitleRole:
            return row.title;
        default:
            return {};
    }
}

QHash<int, QByteArray> AnalyticsObjectBoxesModel::roleNames() const
{
    return {
        {TrackIdRole, "trackId"},
        {ObjectTypeIdRole, "objectTypeId"},
        {TitleRole, "title"},
        {ColorRole, "color"},
        {RectRole, "rect"},
        {IsPointRole, "isPoint"},
    };
}

void AnalyticsObjectBoxesModel::registerQmlType()
{
    qmlRegisterType<AnalyticsObjectBoxesModel>(
        "nx.vms.client.core", 1, 0, "AnalyticsObjectBoxesModel");
}

} // namespace nx::vms::client::core
