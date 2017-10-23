#include "main_window.h"

#include <QtCore/QFileInfo>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlError>
#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/log/log.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace experimental {

class MainWindow::Private: public QObject
{
    MainWindow* const q = nullptr;

public:
    Private(MainWindow* parent);

public:
    QQuickWidget* sceneWidget = nullptr;
};

MainWindow::Private::Private(MainWindow* parent):
    q(parent)
{
}

//-------------------------------------------------------------------------------------------------

MainWindow::MainWindow(QQmlEngine* engine, QnWorkbenchContext* context, QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(context),
    d(new Private(this))
{
    d->sceneWidget = new QQuickWidget(engine, this);

    d->sceneWidget->rootContext()->setContextProperty(lit("workbench"), workbench());
    d->sceneWidget->rootContext()->setContextProperty(
        QnWorkbenchContextAware::kQmlContextPropertyName, context);

    d->sceneWidget->setSource(lit("main.qml"));
    d->sceneWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    if (d->sceneWidget->status() == QQuickWidget::Error)
    {
        for (const auto& error: d->sceneWidget->errors())
            NX_ERROR(this, error.toString());
    }

    d->sceneWidget->show();
}

MainWindow::~MainWindow()
{
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    d->sceneWidget->resize(event->size());
}

} // namespace experimental
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
