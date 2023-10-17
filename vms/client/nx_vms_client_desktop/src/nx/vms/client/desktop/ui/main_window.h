// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/window_context_aware.h>

class QQmlEngine;

namespace nx::vms::client::desktop {
namespace ui {
namespace experimental {

class MainWindow: public QWidget, public WindowContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    MainWindow(QQmlEngine* engine, WindowContext* context, QWidget* parent = nullptr);
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
} // namespace nx::vms::client::desktop
