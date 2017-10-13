#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace nx {
namespace client {
namespace desktop {

class EventSearchWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    EventSearchWidget(QWidget* parent = nullptr);
    virtual ~EventSearchWidget() override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
