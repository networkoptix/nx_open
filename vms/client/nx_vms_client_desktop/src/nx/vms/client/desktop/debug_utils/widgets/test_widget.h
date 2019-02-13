#pragma once

#include <QtCore/QTimer>
#include <QtWidgets/QLabel>

namespace nx::vms::client::desktop {

/**
 * This widget is used for development or research purposes.
 * Add it to complex containers or other situations when you need to know what is going on.
 * Feel free to modify it if needed.
 */
class TestWidget: public QLabel
{
public:
    explicit TestWidget(QWidget* parent = nullptr);

private:
    void updateInfo();

    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

    QScopedPointer<QTimer> m_timer;
};

} // namespace nx::vms::client::desktop