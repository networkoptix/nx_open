#pragma once

#include <QScopedPointer>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QGraphicsProxyWidget>

#include <ui/workbench/workbench_context_aware.h>
#include <core/resource/layout_resource.h>
#include <utils/common/connective.h>

class QLabel;
class QHBoxLayout;

namespace Ui{
class SpecialLayoutPanelWidget;
} // namespace Ui

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

class SpecialLayoutPanelWidget:
    public Connective<QGraphicsProxyWidget>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QGraphicsProxyWidget>;

public:
    SpecialLayoutPanelWidget(const QnLayoutResourcePtr& layoutResource, QObject* parent = nullptr);

    virtual ~SpecialLayoutPanelWidget();

private:
    void handleResourceDataChanged(int role);

    void updateButtons();

private:
    QScopedPointer<Ui::SpecialLayoutPanelWidget> ui;
    QnLayoutResourcePtr m_layoutResource;

    using ButtonPtr = QSharedPointer<QAbstractButton>;
    QList<ButtonPtr> m_actionButtons;
};

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
