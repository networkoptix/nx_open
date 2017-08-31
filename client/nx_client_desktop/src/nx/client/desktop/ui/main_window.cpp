#include "main_window.h"

#include <QtCore/QFileInfo>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlError>

#include <nx/utils/log/log.h>

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
    setSource(QStringLiteral("main.qml"));

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
