#pragma once

#include <QtQuick/QQuickView>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace experimental {

class MainWindow: public QQuickView
{
    Q_OBJECT
    using base_type = QQuickView;

public:
    MainWindow(QQmlEngine* engine, QWindow* parent = nullptr);
    virtual ~MainWindow() override;

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
