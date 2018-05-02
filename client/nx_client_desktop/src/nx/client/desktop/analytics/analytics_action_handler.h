#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

class WorkbenchAnalyticsController;

class AnalyticsActionHandler:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;
public:
    explicit AnalyticsActionHandler(QObject* parent = nullptr);
    virtual ~AnalyticsActionHandler() override;

private:
    void startAnalytics();
    void cleanupControllers();

private:
    using ControllerPtr = QSharedPointer<WorkbenchAnalyticsController>;
    QList<ControllerPtr> m_controllers;
};

} // namespace desktop
} // namespace client
} // namespace nx
