#pragma once

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>

#include "../software_trigger_button.h"

class HoverFocusProcessor;
class QnBusyIndicatorGraphicsWidget;
class QnStyledTooltipWidget;
class QTimer;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace graphics {

class SoftwareTriggerButton;

class SoftwareTriggerButtonPrivate: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_DECLARE_PUBLIC(SoftwareTriggerButton)
    SoftwareTriggerButton* const q_ptr;

public:
    SoftwareTriggerButtonPrivate(SoftwareTriggerButton* main);
    virtual ~SoftwareTriggerButtonPrivate();

    QString toolTip() const;
    void setToolTip(const QString& toolTip);

    Qt::Edge toolTipEdge() const;
    void setToolTipEdge(Qt::Edge edge);

    QSize buttonSize() const;
    void setButtonSize(const QSize& size);

    void setIcon(const QString& name);

    bool prolonged() const;
    void setProlonged(bool value);

    SoftwareTriggerButton::State state() const;
    void setState(SoftwareTriggerButton::State state);

    bool isLive() const;
    bool setLive(bool value); // Returns true if value has changed.

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget);

private:
    void updateToolTipPosition();
    void updateToolTipTailEdge();
    void updateToolTipVisibility();

    void paintCircle(QPainter* painter, const QColor& background,
        const QColor& frame = QColor());

    void paintPixmap(QPainter* painter, const QPixmap& pixmap);

    void scheduleChange(std::function<void()> callback, int delayMs);
    void cancelScheduledChange();

    QPixmap generatePixmap(const QColor& background, const QColor& frame, const QPixmap& icon);
    void ensureImages();

    void updateToolTipText();

    QRect buttonRect() const;

private:
    QString m_iconName;
    QSize m_buttonSize;
    Qt::Edge m_toolTipEdge = Qt::LeftEdge;
    QString m_toolTipText;
    bool m_prolonged = false;
    bool m_live = true;

    QnStyledTooltipWidget* const m_toolTip;
    HoverFocusProcessor* const m_toolTipHoverProcessor;
    SoftwareTriggerButton::State m_state = SoftwareTriggerButton::State::Default;
    QScopedPointer<QnBusyIndicatorGraphicsWidget> m_busyIndicator;
    QPointer<QTimer> m_scheduledChangeTimer = nullptr;
    bool m_imagesDirty = false;

    QElapsedTimer m_animationTime;
    QPixmap m_waitingPixmap;
    QPixmap m_successPixmap;
    QPixmap m_failurePixmap;
    QPixmap m_failureFramePixmap;
    QPixmap m_activityFramePixmap;
    QPixmap m_goToLivePixmap;
    QPixmap m_goToLivePixmapPressed;
};

} // namespace graphics
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
