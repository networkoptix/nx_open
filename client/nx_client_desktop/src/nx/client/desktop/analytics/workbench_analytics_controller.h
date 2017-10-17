#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_data.h>

#include <client_core/connection_context_aware.h>

#include <nx/client/desktop/analytics/analytics_fwd.h>

#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

class WorkbenchAnalyticsController:
    public Connective<QObject>,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    WorkbenchAnalyticsController(
        int matrixSize,
        const QnResourcePtr& resource,
        const AbstractAnalyticsDriverPtr& driver,
        QObject* parent = nullptr);
    virtual ~WorkbenchAnalyticsController() override;

    int matrixSize() const;
    QnResourcePtr resource() const;
    QnLayoutResourcePtr layout() const;

    void addOrChangeRegion(const QnUuid& id, const QRectF& region);
    void removeRegion(const QnUuid& id);

private:
    void constructLayout();
    void updateZoomRect(const QnUuid& itemId, const QRectF& zoomRect);

    /** Adjust rect to source aspect ratio and limit its size. */
    QRectF adjustZoomRect(const QRectF& value) const;

    bool isDynamic() const;

    QnUuid addSlaveItem(const QPoint& position);

private:
    const int m_matrixSize;
    QnResourcePtr m_resource;
    QnLayoutResourcePtr m_layout;
    AbstractAnalyticsDriverPtr m_driver;

    struct ElementData
    {
        QnUuid itemId;
        QnUuid regionId;
    };
    QList<ElementData> m_mapping;

    QnLayoutItemData m_mainItem;
    int m_nextColorIdx = 0;
};

} // namespace desktop
} // namespace client
} // namespace nx
