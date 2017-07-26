#include "workbench_analytics_handler.h"

#include <QtCore/QTimer>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <nx/client/desktop/ui/actions/config.h>
#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_analytics_controller.h>
#include <analytics/metadata_analytics_controller.h>

#include <nx/utils/random.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

static const int kRegionLimit = 12;

class QnDemoAnalyticsDriver: public QnAbstractAnalyticsDriver
{
    using base_type = QnAbstractAnalyticsDriver;
public:
    QnDemoAnalyticsDriver(QObject* parent = nullptr):
        base_type(parent)
    {
        auto moveTimer = new QTimer(this);
        moveTimer->setInterval(16);
        moveTimer->setSingleShot(false);
        connect(moveTimer, &QTimer::timeout, this, &QnDemoAnalyticsDriver::tick);
        moveTimer->start();

        auto addTimer = new QTimer(this);
        addTimer->setInterval(700);
        addTimer->setSingleShot(false);
        connect(addTimer, &QTimer::timeout, this, &QnDemoAnalyticsDriver::addRegionIfNeeded);
        addTimer->start();
    }

private:

    void tick()
    {
        for (auto region = m_regions.begin(); region != m_regions.end(); )
        {
            region->tick();
            if (region->isValid())
            {
                emit regionAddedOrChanged(region->id, region->geometry);
                ++region;
            }
            else
            {
                emit regionRemoved(region->id);
                region = m_regions.erase(region);
            }
        }
    }

    void addRegionIfNeeded()
    {
        if (m_regions.size() >= kRegionLimit)
            return;

        DemoRegion region;
        region.id = QnUuid::createUuid();
        region.geometry.setLeft(-0.1);
        region.geometry.setTop(nx::utils::random::number(-0.5, 0.5));
        region.geometry.setWidth(nx::utils::random::number(0.05, 0.2));
        region.geometry.setHeight(nx::utils::random::number(0.05, 0.2));
        region.offsetPerTick.setX(nx::utils::random::number(0.001, 0.01));
        region.offsetPerTick.setY(nx::utils::random::number(-0.003, 0.003));
        region.resizePerTick.setWidth(nx::utils::random::number(-0.001, 0.001));
        region.resizePerTick.setHeight(nx::utils::random::number(-0.001, 0.001));

        m_regions.push_back(region);
        emit regionAddedOrChanged(region.id, region.geometry);
    }

private:
    struct DemoRegion
    {
        QnUuid id;
        QRectF geometry;
        QPointF offsetPerTick;
        QSizeF resizePerTick;

        void tick()
        {
            geometry.moveTo(geometry.topLeft() + offsetPerTick);

            QSizeF size(geometry.size() + resizePerTick);
            size.setWidth(qBound(0.05, size.width(), 0.9));
            size.setHeight(qBound(0.05, size.height(), 0.9));

            geometry.setSize(size);
        }

        bool isValid() const
        {
            return QRectF(0, 0, 1, 1).intersects(geometry);
        }
    };

    QList<DemoRegion> m_regions;
};


class QnProxyAnalyticsDriver: public QnAbstractAnalyticsDriver
{
    using base_type = QnAbstractAnalyticsDriver;
public:
    QnProxyAnalyticsDriver(const QnVirtualCameraResourcePtr& camera, QObject* parent):
        base_type(parent),
        m_camera(camera)
    {
        connect(qnMetadataAnalyticsController,
            &QnMetadataAnalyticsController::rectangleAddedOrChanged,
            this,
            &QnProxyAnalyticsDriver::proxyRectangleAddedOrChanged);

        connect(qnMetadataAnalyticsController,
            &QnMetadataAnalyticsController::rectangleRemoved,
            this,
            &QnProxyAnalyticsDriver::proxyRectangleRemoved);
    }

public:
    void proxyRectangleRemoved(const QnVirtualCameraResourcePtr& camera, const QnUuid& rectUuid)
    {
        if (camera != m_camera)
            return;

        emit regionRemoved(rectUuid);
    }

    void proxyRectangleAddedOrChanged(const QnVirtualCameraResourcePtr& camera,
        const QnUuid& rectUuid, const QRectF& rect)
    {
        if (camera != m_camera)
            return;

        emit regionAddedOrChanged(rectUuid, rect);
    }

private:
    const QnVirtualCameraResourcePtr m_camera;
};



QnWorkbenchAnalyticsHandler::QnWorkbenchAnalyticsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    if (action::ini().enableAnalytics)
    {
        connect(action(action::StartAnalyticsAction), &QAction::triggered, this,
            &QnWorkbenchAnalyticsHandler::startAnalytics);
    }

    connect(workbench(), &QnWorkbench::layoutsChanged, this,
        &QnWorkbenchAnalyticsHandler::cleanupControllers);
}

QnWorkbenchAnalyticsHandler::~QnWorkbenchAnalyticsHandler()
{
}

void QnWorkbenchAnalyticsHandler::startAnalytics()
{
    action::Parameters parameters = menu()->currentParameters(sender());
    const auto camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    NX_ASSERT(camera);
    if (!camera)
        return;

    int size = parameters.argument(Qn::IntRole).toInt();

    auto existing = std::find_if(m_controllers.cbegin(), m_controllers.cend(),
        [camera, size](const ControllerPtr& controller)
        {
            return controller->matrixSize() == size && controller->camera() == camera;
        });

    if (existing != m_controllers.cend())
    {
        auto layout = (*existing)->layout();
        if (auto existingLayout = QnWorkbenchLayout::instance(layout))
            workbench()->setCurrentLayout(existingLayout);
        else
            menu()->trigger(action::OpenInNewTabAction, layout); //< OpenInSingleLayoutAction
        return;
    }

    ControllerPtr controller(
        new QnWorkbenchAnalyticsController(
            size,
            camera,
            new QnProxyAnalyticsDriver(camera, this)));

    m_controllers.push_back(controller);
    menu()->trigger(action::OpenInNewTabAction, controller->layout());
}

void QnWorkbenchAnalyticsHandler::cleanupControllers()
{
    QSet<QnLayoutResourcePtr> usedLayouts;
    for (auto layout: workbench()->layouts())
        usedLayouts << layout->resource();

    for (auto iter = m_controllers.begin(); iter != m_controllers.end();)
    {
        auto layout = (*iter)->layout();
        if (usedLayouts.contains(layout))
            ++iter;
        else
            iter = m_controllers.erase(iter);
    }
}

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
