#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace nx {
namespace client {
namespace desktop {

class EventListModel;

class NotificationListWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    NotificationListWidget(QWidget* parent = nullptr);
    virtual ~NotificationListWidget() override;

    EventListModel* model() const;
    void setModel(EventListModel* model);

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace
} // namespace client
} // namespace nx
