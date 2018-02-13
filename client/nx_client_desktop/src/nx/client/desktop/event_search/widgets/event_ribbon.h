#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/client/desktop/event_search/widgets/event_tile.h>

class QAbstractListModel;
class QModelIndex;
class QScrollBar;

namespace QnNotificationLevel { enum class Value; }

namespace nx {
namespace client {
namespace desktop {

class EventRibbon: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    EventRibbon(QWidget* parent = nullptr);
    virtual ~EventRibbon() override;

    QAbstractListModel* model() const;
    void setModel(QAbstractListModel* model); //< Use SubsetListModel to proxy non-list model.

    bool showDefaultToolTips() const;
    void setShowDefaultToolTips(bool value);

    bool previewsEnabled() const;
    void setPreviewsEnabled(bool value);

    bool footersEnabled() const;
    void setFootersEnabled(bool value);

    virtual QSize sizeHint() const override;

    QScrollBar* scrollBar() const;

    int count() const;
    int unreadCount() const;

signals:
    void countChanged(int count, QPrivateSignal);
    void unreadCountChanged(int unreadCount, QnNotificationLevel::Value importance, QPrivateSignal);
    void tileHovered(const QModelIndex& index, const EventTile* tile);

protected:
    virtual bool event(QEvent* event) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
