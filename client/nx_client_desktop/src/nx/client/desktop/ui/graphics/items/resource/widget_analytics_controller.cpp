#include "widget_analytics_controller.h"

#include <chrono>

#include <QtCore/QPointer>
#include <QtCore/QElapsedTimer>

#include <nx/utils/random.h>
#include <nx/client/core/media/abstract_analytics_metadata_provider.h>
#include <nx/client/desktop/ui/graphics/items/overlays/area_highlight_overlay_widget.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr std::chrono::milliseconds kObjectTimeToLive = std::chrono::minutes(1);

static const QVector<QColor> kFrameColors{
    Qt::red,
    Qt::green,
    Qt::blue,
    Qt::cyan,
    Qt::magenta,
    Qt::yellow,
    Qt::darkRed,
    Qt::darkGreen,
    Qt::darkBlue,
    Qt::darkCyan,
    Qt::darkMagenta,
    Qt::darkYellow
};

QString objectDescription(const common::metadata::DetectedObject& object)
{
    QString result;

    bool first = true;

    for (const auto& attribute: object.labels)
    {
        if (first)
            first = false;
        else
            result += L'\n';

        result += attribute.name;
        result += L'\t';
        result += attribute.value;
    }

    return result;
}

} // namespace

class WidgetAnalyticsController::Private
{
public:
    core::AbstractAnalyticsMetadataProviderPtr metadataProvider;
    QPointer<AreaHighlightOverlayWidget> areaHighlightWidget;

    struct ObjectInfo
    {
        QColor color;
        bool active = true;
        qint64 lastUsedTime = -1;
    };

    QHash<QnUuid, ObjectInfo> objectInfoById;
    QElapsedTimer timer;
};

WidgetAnalyticsController::WidgetAnalyticsController():
    d(new Private())
{
    d->timer.restart();
}

WidgetAnalyticsController::~WidgetAnalyticsController()
{
}

void WidgetAnalyticsController::updateAreas(qint64 timestamp, int channel)
{
    if (!d->metadataProvider || !d->areaHighlightWidget)
        return;

    const auto elapsed = d->timer.elapsed();

    const auto metadata = d->metadataProvider->metadata(timestamp, channel);

    QSet<QnUuid> objectIds;

    if (metadata)
    {
        for (const auto& object: metadata->objects)
        {
            auto& objectInfo = d->objectInfoById[object.objectId];
            if (objectInfo.lastUsedTime < 0)
                objectInfo.color = utils::random::choice(kFrameColors);

            objectInfo.active = true;
            objectInfo.lastUsedTime = elapsed;

            AreaHighlightOverlayWidget::AreaInformation areaInfo;
            areaInfo.id = object.objectId;
            areaInfo.rectangle = object.boundingBox;
            areaInfo.color = objectInfo.color;
            areaInfo.text = objectDescription(object);

            d->areaHighlightWidget->addOrUpdateArea(areaInfo);

            objectIds.insert(object.objectId);
        }
    }

    for (auto it = d->objectInfoById.begin(); it != d->objectInfoById.end(); /*no increment*/)
    {
        if (!objectIds.contains(it.key()) && it->active)
        {
            d->areaHighlightWidget->removeArea(it.key());
            it->active = false;
        }

        if (it->active || elapsed - it->lastUsedTime < kObjectTimeToLive.count())
            ++it;
        else
            it = d->objectInfoById.erase(it);
    }
}

void WidgetAnalyticsController::clearAreas()
{
    for (auto it = d->objectInfoById.begin(); it != d->objectInfoById.end(); ++it)
    {
        it->active = false;
        d->areaHighlightWidget->removeArea(it.key());
    }
}

void WidgetAnalyticsController::setAnalyticsMetadataProvider(
    const core::AbstractAnalyticsMetadataProviderPtr& provider)
{
    d->metadataProvider = provider;
}

void WidgetAnalyticsController::setAreaHighlightOverlayWidget(AreaHighlightOverlayWidget* widget)
{
    d->areaHighlightWidget = widget;
}

} // namespace desktop
} // namespace client
} // namespace nx
