#include "workbench_analytics_controller.h"

#include <client/client_globals.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_runtime_data.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <nx/client/core/utils/geometry.h>
#include <ui/graphics/items/resource/resource_widget.h> //TODO: #GDM move enum to client globals
#include <ui/style/skin.h>

using nx::client::core::Geometry;

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
namespace ui {

QnAbstractAnalyticsDriver::QnAbstractAnalyticsDriver(QObject* parent /*= nullptr*/):
    QObject(parent)
{
}

QnWorkbenchAnalyticsController::QnWorkbenchAnalyticsController(
    int matrixSize,
    const QnVirtualCameraResourcePtr& camera,
    QnAbstractAnalyticsDriver* driver,
    QObject* parent)
    :
    base_type(parent),
    m_matrixSize(matrixSize),
    m_camera(camera)
{
    constructLayout();

    if (driver)
    {
        connect(driver, &QnAbstractAnalyticsDriver::regionAddedOrChanged, this,
            &QnWorkbenchAnalyticsController::addOrChangeRegion);
        connect(driver, &QnAbstractAnalyticsDriver::regionRemoved, this,
            &QnWorkbenchAnalyticsController::removeRegion);
    }
}

QnWorkbenchAnalyticsController::~QnWorkbenchAnalyticsController()
{
}

int QnWorkbenchAnalyticsController::matrixSize() const
{
    return m_matrixSize;
}

QnVirtualCameraResourcePtr QnWorkbenchAnalyticsController::camera() const
{
    return m_camera;
}

QnLayoutResourcePtr QnWorkbenchAnalyticsController::layout() const
{
    return m_layout;
}

void QnWorkbenchAnalyticsController::addOrChangeRegion(const QnUuid& id, const QRectF& region)
{
    auto result = m_mapping.end();
    for (auto iter = m_mapping.begin(); iter != m_mapping.end(); ++iter)
    {
        // Item is already controlled.
        if (iter->regionId == id)
        {
            result = iter;
            break;
        }

        // First free item.
        else if (result == m_mapping.end() && iter->regionId.isNull())
        {
            result = iter;
        }
    }

    if (result == m_mapping.end())
    {
        if (!isDynamic())
            return;

        ElementData data;
        data.itemId = addSlaveItem(QPoint());
        m_mapping.push_back(data);
        result = m_mapping.end() - 1;
    }

    result->regionId = id;
    updateZoomRect(result->itemId, region);
}

void QnWorkbenchAnalyticsController::removeRegion(const QnUuid& id)
{
    auto iter = std::find_if(m_mapping.begin(), m_mapping.end(),
        [id](const ElementData& data)
        {
            return data.regionId == id;
        });

    if (iter == m_mapping.end())
        return;

    if (isDynamic())
    {
        m_layout->removeItem(iter->itemId);
        m_mapping.erase(iter);
    }
    else
    {
        iter->regionId = QnUuid();
        updateZoomRect(iter->itemId, QRectF());
    }
}

void QnWorkbenchAnalyticsController::constructLayout()
{
    const int centralItemSize = (m_matrixSize + 1) / 2;
    const int centralItemPosition = (m_matrixSize - centralItemSize) / 2;
    const QRect centralItemRect = isDynamic()
        ? QRect(0, 0, 2, 2)
        : QRect(centralItemPosition, centralItemPosition, centralItemSize, centralItemSize);

    // Construct and add a new layout.
    m_layout.reset(new QnLayoutResource());

    m_layout->addFlags(Qn::local);
    m_layout->setCellSpacing(kDefaultCellSpacing);

    auto ar = m_camera->aspectRatio();
    if (ar.isValid())
        m_layout->setCellAspectRatio(ar.toFloat());

    m_layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission));

    auto updateName = [this]
        {
            m_layout->setName(tr("%1 Analytics").arg(m_camera->getName()));
        };
    updateName();
    connect(m_camera, &QnResource::nameChanged, this, updateName);

    m_layout->setId(QnUuid::createUuid());
    m_layout->setData(Qt::DecorationRole, qnSkin->icon("layouts/preview_search.png"));

    // Add main item.
    m_mainItem.flags = Qn::Pinned;
    m_mainItem.uuid = QnUuid::createUuid();
    m_mainItem.combinedGeometry = centralItemRect;
    m_mainItem.resource.id = m_camera->getId();
    m_mainItem.resource.uniqueId = m_camera->getUniqueId();
    qnResourceRuntimeDataManager->setLayoutItemData(m_mainItem.uuid,
        Qn::ItemWidgetOptions,
        kMasterItemOptions);
    m_layout->addItem(m_mainItem);

    if (!isDynamic())
    {
        // Add zoom windows.
        for (int x = 0; x < m_matrixSize; ++x)
        {
            for (int y = 0; y < m_matrixSize; ++y)
            {
                if (centralItemRect.contains(x, y))
                    continue;

                ElementData element;
                element.itemId = addSlaveItem(QPoint(x, y));
                m_mapping.push_back(element);
            }
        }
    }

    resourcePool()->addResource(m_layout);
}

void QnWorkbenchAnalyticsController::updateZoomRect(const QnUuid& itemId, const QRectF& zoomRect)
{
    NX_ASSERT(!itemId.isNull());
    if (itemId.isNull())
        return;

    QnLayoutItemData item = m_layout->getItem(itemId);
    NX_ASSERT(!item.uuid.isNull());
    if (item.uuid.isNull())
        return; // Item was not found for some unknown reason.

    qnResourceRuntimeDataManager->setLayoutItemData(item.uuid,
        Qn::ItemAnalyticsModeSourceRegionRole,
        zoomRect);
    item.zoomRect = adjustZoomRect(zoomRect);
    m_layout->updateItem(item);
}

QRectF QnWorkbenchAnalyticsController::adjustZoomRect(const QRectF& value) const
{
    if (value.isEmpty())
        return value;

    static const QRectF kFullRect(0, 0, 1, 1);

    QRectF result(value);
    result = result.intersected(kFullRect);
    if (result.isEmpty())
        return result;

    // Zoom rects are stored in relative coordinates, so aspect ratio must be 1.0
    result = Geometry::expanded(1.0, result, Qt::KeepAspectRatioByExpanding);
    result = Geometry::movedInto(result, kFullRect);
    return result;
}

bool QnWorkbenchAnalyticsController::isDynamic() const
{
    return m_matrixSize <= 0;
}

QnUuid QnWorkbenchAnalyticsController::addSlaveItem(const QPoint& position)
{
    static const QSize kDefaultItemSize(1, 1);

    QnLayoutItemData item;
    item.uuid = QnUuid::createUuid();
    if (isDynamic())
    {
        item.flags = Qn::PendingGeometryAdjustment;
        QPointF targetPoint = m_mainItem.combinedGeometry.bottomRight();
        item.combinedGeometry = QRectF(targetPoint, targetPoint);
    }
    else
    {
        item.flags = Qn::Pinned;
        item.combinedGeometry = QRect(position, kDefaultItemSize);
    }
    item.resource = m_mainItem.resource;
    item.zoomRect = QRectF();
    item.zoomTargetUuid = m_mainItem.uuid;
    qnResourceRuntimeDataManager->setLayoutItemData(item.uuid,
        Qn::ItemFrameDistinctionColorRole,
        kFrameColors[m_nextColorIdx]);

    m_nextColorIdx = (m_nextColorIdx + 1) % kFrameColors.size();

    qnResourceRuntimeDataManager->setLayoutItemData(item.uuid,
        Qn::ItemWidgetOptions,
        kSlaveItemOptions);

    m_layout->addItem(item);
    return item.uuid;
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
