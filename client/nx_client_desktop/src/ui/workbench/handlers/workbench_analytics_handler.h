#pragma once

#include <core/resource/resource_fwd.h>
#include <client_core/connection_context_aware.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class QnWorkbenchAnalyticsController;

class QnWorkbenchAnalyticsHandler:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
public:
    QnWorkbenchAnalyticsHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchAnalyticsHandler() override;

private:
    void startAnalytics();
    void cleanupControllers();

private:
    using ControllerPtr = QSharedPointer<QnWorkbenchAnalyticsController>;
    QList<ControllerPtr> m_controllers;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
