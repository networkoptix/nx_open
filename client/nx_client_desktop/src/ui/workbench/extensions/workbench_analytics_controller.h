#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_data.h>

#include <client_core/connection_context_aware.h>

#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class QnAbstractAnalyticsDriver: public QObject
{
    Q_OBJECT
public:
    QnAbstractAnalyticsDriver(QObject* parent = nullptr);
signals:
    void regionAddedOrChanged(const QnUuid& id, const QRectF& region);
    void regionRemoved(const QnUuid& id);
};

class QnWorkbenchAnalyticsController:
    public Connective<QObject>,
    public QnConnectionContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    QnWorkbenchAnalyticsController(
        int matrixSize,
        const QnVirtualCameraResourcePtr& camera,
        QnAbstractAnalyticsDriver* driver,
        QObject* parent = nullptr);
    virtual ~QnWorkbenchAnalyticsController() override;

    int matrixSize() const;
    QnVirtualCameraResourcePtr camera() const;
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
    QnVirtualCameraResourcePtr m_camera;
    QnLayoutResourcePtr m_layout;

    struct ElementData
    {
        QnUuid itemId;
        QnUuid regionId;
    };
    QList<ElementData> m_mapping;

    QnLayoutItemData m_mainItem;
    int m_nextColorIdx = 0;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
