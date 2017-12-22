#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/widgets/common/panel.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {

class EventPanel:
    public QnPanel,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnPanel;

public:
    explicit EventPanel(QWidget* parent);
    explicit EventPanel(QnWorkbenchContext* context, QWidget* parent = nullptr);

    virtual ~EventPanel() override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
