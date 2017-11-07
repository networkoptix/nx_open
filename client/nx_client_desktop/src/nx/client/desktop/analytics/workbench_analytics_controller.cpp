#include "workbench_analytics_controller.h"

#include <QtCore/QFileInfo>

#include <ini.h>

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <nx/client/core/utils/grid_walker.h>

#include <nx/client/desktop/analytics/drivers/abstract_analytics_driver.h>
#include <nx/client/desktop/layout_templates/layout_template.h>
#include <nx/client/desktop/layout_templates/template_layout_builder.h>

#include <ui/common/geometry.h>
#include <ui/graphics/items/resource/resource_widget.h> //TODO: #GDM move enum to client globals
#include <ui/style/skin.h>

namespace {

static constexpr auto kDefaultCellSpacing = 0.05;

static const QList<QColor> kFrameColors{
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

static const QnResourceWidget::Options kMasterItemOptions =
    QnResourceWidget::AnalyticsModeMaster;

static const QnResourceWidget::Options kSlaveItemOptions =
    QnResourceWidget::WindowRotationForbidden |
    QnResourceWidget::InfoOverlaysForbidden |
    QnResourceWidget::AnalyticsModeSlave;

} // namespace


namespace nx {
namespace client {
namespace desktop {

class WorkbenchAnalyticsController::Private: public Connective<QObject>
{
    WorkbenchAnalyticsController* const q;

public:
    struct ElementData
    {
        QnUuid itemId;
        QnUuid regionId;
    };

    struct ElementMapping
    {
        QList<ElementData> mapping;
        QnLayoutItemData source;
        int nextColorIdx = 0;
    };

    int matrixSize = 0;
    LayoutTemplate layoutTemplate;
    QnResourcePtr resource;
    QnLayoutResourcePtr layout;
    AbstractAnalyticsDriverPtr driver;

    ElementMapping mainMapping;
    ElementMapping enhancedMapping;

public:
    Private(WorkbenchAnalyticsController* parent);
    virtual ~Private() override;

    QnResourcePtr findEnhancedResource(const QnResourcePtr& resource) const;

    void setupLayout();
    void constructLayout();
    void constructLayoutFromTemplate();
    void updateZoomRect(const QnUuid& itemId, const QnUuid& regionId, const QRectF& zoomRect);

    /** Adjust rect to source aspect ratio and limit its size. */
    QRectF adjustZoomRect(const QRectF& value) const;

    bool isDynamic() const;

    QnUuid addSlaveItem(ElementMapping& source, const QPoint& position);

    void connectToDriver();
};

WorkbenchAnalyticsController::Private::Private(WorkbenchAnalyticsController* parent):
    q(parent)
{
}

WorkbenchAnalyticsController::Private::~Private()
{
}

QnResourcePtr WorkbenchAnalyticsController::Private::findEnhancedResource(
    const QnResourcePtr& resource) const
{
    QString enhancedVideoName = resource->getUrl();
    const auto extension = QFileInfo(enhancedVideoName).suffix();
    enhancedVideoName.replace(extension, lit("enhanced.") + extension);
    return q->resourcePool()->getResourceByUrl(enhancedVideoName);
}

void WorkbenchAnalyticsController::Private::setupLayout()
{
    layout->addFlags(Qn::local);
    layout->setCellSpacing(kDefaultCellSpacing);
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission));

    auto updateName =
        [this]
        {
            layout->setName(tr("%1 Analytics").arg(resource->getName()));
        };
    updateName();
    connect(resource, &QnResource::nameChanged, this, updateName);

    layout->setId(QnUuid::createUuid());
    layout->setData(Qt::DecorationRole, qnSkin->icon("layouts/preview_search.png"));
}

void WorkbenchAnalyticsController::Private::constructLayout()
{
    const int centralItemSize = (matrixSize + 1) / 2;
    const int centralItemPosition = (matrixSize - centralItemSize) / 2;
    const QRect centralItemRect = isDynamic()
        ? QRect(0, 0, 2, 2)
        : QRect(centralItemPosition, centralItemPosition, centralItemSize, centralItemSize);
    const int enhancedOffset = matrixSize;

    // Construct and add a new layout.
    layout.reset(new QnLayoutResource());

    setupLayout();

    // Add main item.
    mainMapping.source.flags = Qn::Pinned;
    mainMapping.source.uuid = QnUuid::createUuid();
    mainMapping.source.combinedGeometry = centralItemRect;
    mainMapping.source.resource.id = resource->getId();
    mainMapping.source.resource.uniqueId = resource->getUniqueId();
    qnResourceRuntimeDataManager->setLayoutItemData(mainMapping.source.uuid,
        Qn::ItemWidgetOptions,
        kMasterItemOptions);
    layout->addItem(mainMapping.source);

    if (ini().enableEntropixEnhancer)
    {
        if (const auto& enhancedResource = findEnhancedResource(resource))
        {
            resource->addFlags(Qn::sync);
            enhancedResource->addFlags(Qn::sync);

            auto enhancedItemOptions = kMasterItemOptions;
            int positionX = centralItemPosition + enhancedOffset;
            if (ini().hideEnhancedVideo)
            {
                enhancedItemOptions |= QnResourceWidget::InvisibleWidgetOption
                   | QnResourceWidget::InfoOverlaysForbidden;
                positionX = centralItemPosition; //< Hide below the main item;
            }

            enhancedMapping.source.flags = Qn::Pinned;
            enhancedMapping.source.uuid = QnUuid::createUuid();
            enhancedMapping.source.resource.id = enhancedResource->getId();
            enhancedMapping.source.resource.uniqueId = enhancedResource->getUniqueId();
            enhancedMapping.source.combinedGeometry = QRect(positionX,
                centralItemPosition, centralItemSize, centralItemSize);

            qnResourceRuntimeDataManager->setLayoutItemData(enhancedMapping.source.uuid,
                Qn::ItemWidgetOptions,
                enhancedItemOptions);

            layout->addItem(enhancedMapping.source);
        }
    }

    if (!isDynamic())
    {
        const bool hasEnhanced =
            ini().enableEntropixEnhancer && !enhancedMapping.source.uuid.isNull();

        // Add zoom windows.

        if (hasEnhanced && ini().hideEnhancedVideo)
        {
            core::GridWalker w(QRect(-1, -1, matrixSize + 1, matrixSize + 1),
                core::GridWalker::Policy::Round);

            bool addEnhanced = false;
            while (w.next())
            {
                if (centralItemRect.contains(w.pos()))
                    continue;

                auto& mapping = addEnhanced ? enhancedMapping : mainMapping;
                ElementData element;
                element.itemId = addSlaveItem(mapping, w.pos());
                mapping.mapping.push_back(element);
                addEnhanced = !addEnhanced;
            }
        }
        else
        {
            core::GridWalker w(QRect(0, 0, matrixSize, matrixSize));
            while (w.next())
            {
                if (centralItemRect.contains(w.pos()))
                    continue;

                ElementData element;
                element.itemId = addSlaveItem(mainMapping, w.pos());
                mainMapping.mapping.push_back(element);

                if (hasEnhanced)
                {
                    element.itemId = addSlaveItem(enhancedMapping, w.pos() + QPoint(enhancedOffset, 0));
                    enhancedMapping.mapping.push_back(element);
                }
            }
        }
    }
}

void WorkbenchAnalyticsController::Private::constructLayoutFromTemplate()
{
    TemplateLayoutBuilder builder;
    builder.setResources(LayoutTemplateItem::kCameraSourceType, {resource});

    if (ini().enableEntropixEnhancer)
    {
        if (const auto& enhancedResource = findEnhancedResource(resource))
        {
            resource->addFlags(Qn::sync);
            enhancedResource->addFlags(Qn::sync);

            builder.setResources(
                LayoutTemplateItem::kEnhancedCameraSourceType, {enhancedResource});
        }
    }

    layout = builder.buildLayout(layoutTemplate);

    if (layout)
        setupLayout();

    mainMapping.mapping.clear();
    enhancedMapping.mapping.clear();

    for (const auto& item: builder.createdItems())
    {
        if (item.block != 0)
            continue;

        if (item.itemTemplate.type == LayoutTemplateItem::kResourceItemType)
        {
            if (item.itemTemplate.source.type == LayoutTemplateItem::kCameraSourceType)
                mainMapping.source = item.data;
            else if (item.itemTemplate.source.type == LayoutTemplateItem::kEnhancedCameraSourceType)
                enhancedMapping.source = item.data;
        }
        else if (item.itemTemplate.type == LayoutTemplateItem::kZoomWindowItemType)
        {
            const ElementData elementData{item.data.uuid, QnUuid()};

            if (item.data.zoomTargetUuid == mainMapping.source.uuid)
                mainMapping.mapping.append(elementData);
            else if (item.data.zoomTargetUuid == enhancedMapping.source.uuid)
                enhancedMapping.mapping.append(elementData);
        }
    }
}

void WorkbenchAnalyticsController::Private::updateZoomRect(const QnUuid& itemId,
    const QnUuid& regionId,
    const QRectF& zoomRect)
{
    NX_ASSERT(!itemId.isNull());
    if (itemId.isNull())
        return;

    QnLayoutItemData item = layout->getItem(itemId);
    NX_ASSERT(!item.uuid.isNull());
    if (item.uuid.isNull())
        return; // Item was not found for some unknown reason.

    qnResourceRuntimeDataManager->setLayoutItemData(item.uuid,
        Qn::ItemAnalyticsModeSourceRegionRole,
        zoomRect);
    qnResourceRuntimeDataManager->setLayoutItemData(item.uuid,
        Qn::ItemAnalyticsModeRegionIdRole,
        regionId);

    // Enhanced videos load their metadata themselves.
    const bool hasEnhanced = ini().enableEntropixEnhancer && !enhancedMapping.source.uuid.isNull();
    if (hasEnhanced)
        return;

    item.zoomRect = adjustZoomRect(zoomRect);
    layout->updateItem(item);
}

QRectF WorkbenchAnalyticsController::Private::adjustZoomRect(const QRectF& value) const
{
    if (value.isEmpty())
        return value;

    static const QRectF kFullRect(0, 0, 1, 1);

    QRectF result(value);
    result = result.intersected(kFullRect);
    if (result.isEmpty())
        return result;

    // Zoom rects are stored in relative coordinates, so aspect ratio must be 1.0
    if (!ini().allowCustomArZoomWindows)
        result = QnGeometry::expanded(1.0, result, Qt::KeepAspectRatioByExpanding);
    result = QnGeometry::movedInto(result, kFullRect);
    return result;
}

bool WorkbenchAnalyticsController::Private::isDynamic() const
{
    return !layoutTemplate.isValid() && matrixSize <= 0;
}

QnUuid WorkbenchAnalyticsController::Private::addSlaveItem(
    ElementMapping& mapping, const QPoint& position)
{
    static const QSize kDefaultItemSize(1, 1);

    QnLayoutItemData item;
    item.uuid = QnUuid::createUuid();
    if (isDynamic())
    {
        item.flags = Qn::PendingGeometryAdjustment;
        QPointF targetPoint = mapping.source.combinedGeometry.bottomRight();
        item.combinedGeometry = QRectF(targetPoint, targetPoint);
    }
    else
    {
        item.flags = Qn::Pinned;
        item.combinedGeometry = QRect(position, kDefaultItemSize);
    }
    item.resource = mapping.source.resource;
    item.zoomRect = QRectF();
    item.zoomTargetUuid = mapping.source.uuid;
    qnResourceRuntimeDataManager->setLayoutItemData(item.uuid,
        Qn::ItemFrameDistinctionColorRole,
        kFrameColors[mapping.nextColorIdx]);

    mapping.nextColorIdx = (mapping.nextColorIdx + 1) % kFrameColors.size();

    qnResourceRuntimeDataManager->setLayoutItemData(item.uuid,
        Qn::ItemWidgetOptions,
        kSlaveItemOptions);

    layout->addItem(item);
    return item.uuid;
}

void WorkbenchAnalyticsController::Private::connectToDriver()
{
    if (!driver)
        return;

    connect(driver, &AbstractAnalyticsDriver::regionAddedOrChanged, q,
        &WorkbenchAnalyticsController::addOrChangeRegion);
    connect(driver, &AbstractAnalyticsDriver::regionRemoved, q,
        &WorkbenchAnalyticsController::removeRegion);
}

//-------------------------------------------------------------------------------------------------

WorkbenchAnalyticsController::WorkbenchAnalyticsController(
    int matrixSize,
    const QnResourcePtr& resource,
    const AbstractAnalyticsDriverPtr& driver,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this))
{
    d->matrixSize = matrixSize;
    d->resource = resource;
    d->driver = driver;

    d->constructLayout();
    d->connectToDriver();
}

WorkbenchAnalyticsController::WorkbenchAnalyticsController(
    const LayoutTemplate& layoutTemplate,
    const QnResourcePtr& resource,
    const AbstractAnalyticsDriverPtr& driver,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(this))
{
    d->layoutTemplate = layoutTemplate;
    d->resource = resource;
    d->driver = driver;

    d->constructLayoutFromTemplate();
    d->connectToDriver();
}

WorkbenchAnalyticsController::~WorkbenchAnalyticsController()
{
}

int WorkbenchAnalyticsController::matrixSize() const
{
    return d->matrixSize;
}

QnResourcePtr WorkbenchAnalyticsController::resource() const
{
    return d->resource;
}

QnLayoutResourcePtr WorkbenchAnalyticsController::layout() const
{
    return d->layout;
}

const LayoutTemplate& WorkbenchAnalyticsController::layoutTemplate() const
{
    return d->layoutTemplate;
}

void WorkbenchAnalyticsController::addOrChangeRegion(const QnUuid& id, const QRectF& region)
{
    auto addOrUpdateRegionInternal = [this, id, region](Private::ElementMapping& mapping)
        {
            auto result = mapping.mapping.end();
            for (auto iter = mapping.mapping.begin(); iter != mapping.mapping.end(); ++iter)
            {
                // Item is already controlled.
                if (iter->regionId == id)
                {
                    result = iter;
                    break;
                }

                // First free item.
                if (result == mapping.mapping.end() && iter->regionId.isNull())
                    result = iter;
            }

            if (result == mapping.mapping.end())
            {
                if (!d->isDynamic())
                    return;

                Private::ElementData data;
                data.itemId = d->addSlaveItem(mapping, QPoint());
                mapping.mapping.push_back(data);
                result = mapping.mapping.end() - 1;
            }

            result->regionId = id;
            d->updateZoomRect(result->itemId, result->regionId, region);
        };

    addOrUpdateRegionInternal(d->mainMapping);
    if (ini().enableEntropixEnhancer && !d->enhancedMapping.source.uuid.isNull())
        addOrUpdateRegionInternal(d->enhancedMapping);
}

void WorkbenchAnalyticsController::removeRegion(const QnUuid& id)
{
    auto removeRegionInternal =
        [this, id](Private::ElementMapping& mapping)
        {

            auto iter = std::find_if(mapping.mapping.begin(), mapping.mapping.end(),
                [id](const Private::ElementData& data)
            {
                return data.regionId == id;
            });

            if (iter == mapping.mapping.end())
                return;

            if (d->isDynamic())
            {
                d->layout->removeItem(iter->itemId);
                mapping.mapping.erase(iter);
            }
            else
            {
                iter->regionId = QnUuid();
                d->updateZoomRect(iter->itemId, QnUuid(), QRectF());
            }
        };

    removeRegionInternal(d->mainMapping);
    if (ini().enableEntropixEnhancer && !d->enhancedMapping.source.uuid.isNull())
        removeRegionInternal(d->enhancedMapping);
}

} // namespace desktop
} // namespace client
} // namespace nx
