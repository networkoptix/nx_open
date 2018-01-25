#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

class QAbstractListModel;
class QModelIndex;
class QScrollBar;

namespace QnNotificationLevel { enum class Value; }

namespace nx {
namespace client {
namespace desktop {

class EventTile;

class EventRibbon: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    EventRibbon(QWidget* parent = nullptr);
    virtual ~EventRibbon() override;

    QAbstractListModel* model() const;
    void setModel(QAbstractListModel* model); //< Use SubsetListModel to proxy non-list model.

    virtual QSize sizeHint() const override;

    QScrollBar* scrollBar() const;

    int count() const;
    int unreadCount() const;

signals:
    void countChanged(int count);
    void unreadCountChanged(int unreadCount, QnNotificationLevel::Value importance);

protected:
    virtual bool event(QEvent* event) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
