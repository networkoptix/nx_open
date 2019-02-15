#include "test_widget.h"

#include  <QtGui/QMoveEvent>
#include  <QtGui/QResizeEvent>

namespace nx::vms::client::desktop {

TestWidget::TestWidget(QWidget* parent):
    QLabel(parent),
    m_timer(new QTimer(this))
{
    setFrameShape(QFrame::Box);
    connect(m_timer.data(), &QTimer::timeout, this, &TestWidget::updateInfo);
    m_timer->start(100);
}

void TestWidget::updateInfo()
{
    setText(QString("%1 %2").arg(x()).arg(y()));
}

void TestWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event); // Place breakpoint here.
}

void TestWidget::moveEvent(QMoveEvent* event)
{
    if (event->pos().x() > 0)
    {
        []() {}();
//        move(0, pos().y());
    }
    QWidget::moveEvent(event); // Place breakpoint here.
}

}
