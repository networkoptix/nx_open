#pragma once

#include <chrono>

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <nx/utils/interval.h>
#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>

class QAbstractListModel;
class QModelIndex;
class QScrollBar;

namespace QnNotificationLevel { enum class Value; }

namespace nx::vms::client::desktop {

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

    Qt::ScrollBarPolicy scrollBarPolicy() const;
    void setScrollBarPolicy(Qt::ScrollBarPolicy value);

    bool live() const;
    void setLive(bool value);

    std::chrono::microseconds highlightedTimestamp() const;
    void setHighlightedTimestamp(std::chrono::microseconds value);

    void setViewportMargins(int top, int bottom);

    virtual QSize sizeHint() const override;

    QScrollBar* scrollBar() const;

    int count() const;
    int unreadCount() const;

    nx::utils::Interval<int> visibleRange() const;

signals:
    void countChanged(int count);
    void unreadCountChanged(int unreadCount, QnNotificationLevel::Value importance, QPrivateSignal);
    void visibleRangeChanged(const nx::utils::Interval<int>& value, QPrivateSignal);

    // Tile interaction.
    void hovered(const QModelIndex& index, EventTile* tile);
    void clicked(const QModelIndex& index, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void doubleClicked(const QModelIndex& index); //< With left mouse button.
    void dragStarted(const QModelIndex& index, const QPoint& pos, const QSize& size);
    void linkActivated(const QModelIndex& index, const QString& link);

protected:
    virtual bool event(QEvent* event) override;

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
