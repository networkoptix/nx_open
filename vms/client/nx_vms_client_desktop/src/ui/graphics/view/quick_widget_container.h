// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>
#include <QtQuickWidgets/QQuickWidget>

namespace nx::vms::client::desktop {

/**
 * A widget completely covered by QQuickWidget, used as a workaround for main window scene
 * rendering issues, such as panel backgrounds drawing incorrectly.
 */
class QuickWidgetContainer: public QWidget
{
    Q_OBJECT

    using base_type = QWidget;

public:
    using base_type::base_type;

    void setView(QWidget* view);
    QWidget* view() const;

    void setQuickWidget(QQuickWidget* quickWidget);
    QQuickWidget* quickWidget() const;

    QPaintEngine* paintEngine() const override;

protected:
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    bool event(QEvent* event) override;

private:
    QPointer<QQuickWidget> m_quickWidget;
    QPointer<QWidget> m_view;
};

} // namespace nx::vms::client::desktop
