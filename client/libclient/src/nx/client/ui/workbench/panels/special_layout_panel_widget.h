#pragma once

#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QGraphicsProxyWidget>

#include <ui/workbench/workbench_context_aware.h>
#include <core/resource/layout_resource.h>
#include <utils/common/connective.h>

class QLabel;
class QHBoxLayout;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class SpecialLayoutPanelWidget: public Connective<QGraphicsProxyWidget>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QGraphicsProxyWidget>;

public:
    SpecialLayoutPanelWidget(const QnLayoutResourcePtr& layoutResource, QObject* parent = nullptr);

private:
    void handleResourceDataChanged(int role);

    void updateButtons();

private:
    QLabel* m_caption = nullptr;
    QLabel* m_description = nullptr;
    QHBoxLayout* m_layout = nullptr;
    QnLayoutResourcePtr m_layoutResource;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
