#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

/**
 * A custom title bar which obtains its state (caption, icon, visible buttons and their state)
 * from a top-level parent window, and connects its buttons to standard slots of the window.
 */
class DialogTitleBarWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    DialogTitleBarWidget(QWidget* parent = nullptr);
    virtual ~DialogTitleBarWidget() override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
