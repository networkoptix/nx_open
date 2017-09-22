#pragma once

#include <QtWidgets/QWidget>

#include <ui/workbench/workbench_context_aware.h>

class QQmlEngine;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace experimental {

class MainWindow: public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    MainWindow(QQmlEngine* engine, QnWorkbenchContext* context, QWidget* parent = nullptr);
    virtual ~MainWindow() override;

protected:
    virtual void resizeEvent(QResizeEvent* event) override;

private:
    class Private;
    QScopedPointer<Private> d;
    friend class Private;
};

} // namespace experimental
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
