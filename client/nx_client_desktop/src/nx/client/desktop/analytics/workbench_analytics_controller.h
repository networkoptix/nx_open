#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_data.h>

#include <client_core/connection_context_aware.h>

#include <nx/client/desktop/analytics/analytics_fwd.h>

#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

struct LayoutTemplate;

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

    WorkbenchAnalyticsController(
        const LayoutTemplate& layoutTemplate,
        const QnResourcePtr& resource,
        const AbstractAnalyticsDriverPtr& driver,
        QObject* parent = nullptr);

    virtual ~WorkbenchAnalyticsController() override;

    int matrixSize() const;
    QnResourcePtr resource() const;
    QnLayoutResourcePtr layout() const;
    const LayoutTemplate& layoutTemplate() const;

    void addOrChangeRegion(const QnUuid& id, const QRectF& region);
    void removeRegion(const QnUuid& id);

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace desktop
} // namespace client
} // namespace nx
