#include "main_window.h"

#include <QtQml/QQmlError>

#include <nx/utils/log/log.h>

#include <client/client_module.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace experimental {

static const QString kQmlSource = QStringLiteral("qrc:///qml/main.qml");

class MainWindow::Private: public QObject
{
    MainWindow* const q = nullptr;

public:
    Private(MainWindow* parent);
};

MainWindow::Private::Private(MainWindow* parent):
    q(parent)
{
}

//-------------------------------------------------------------------------------------------------

MainWindow::MainWindow(QQmlEngine* engine, QWindow* parent):
    base_type(engine, parent),
    d(new Private(this))
{
    const auto qmlRoot = qnClientModule->startupParameters().qmlRoot;
    const auto sourceUrl = qmlRoot.isEmpty() ? kQmlSource : qmlRoot;

    NX_DEBUG(this, lm("Loading from %1").arg(sourceUrl));
    setSource(sourceUrl);

    if (status() == Error)
    {
        for (const auto& error: errors())
            NX_ERROR(this, error.toString());
    }
}

MainWindow::~MainWindow()
{
}

} // namespace experimental
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
