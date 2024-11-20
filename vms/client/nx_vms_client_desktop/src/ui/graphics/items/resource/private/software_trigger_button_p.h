// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>

#include "../software_trigger_button.h"

class HoverFocusProcessor;
class QnStyledTooltipWidget;
class QTimer;

namespace nx::vms::client::desktop {

class BusyIndicatorGraphicsWidget;
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

    void setIcon(const QString& name);

    void updateState();

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget);

    void updateToolTipText();

private:
    void updateToolTipPosition();
    void updateToolTipTailEdge();
    void updateToolTipVisibility();

    void paintCircle(QPainter* painter, const QColor& background,
        const QColor& frame = QColor());

    void paintPixmap(QPainter* painter, const QPixmap& pixmap);

    void scheduleChange(std::function<void()> callback, int delayMs);
    void cancelScheduledChange();

    void ensureImages();

private:
    Qt::Edge m_toolTipEdge = Qt::LeftEdge;
    QString m_toolTipText;
    bool m_prolonged = false;

    QnStyledTooltipWidget* const m_toolTip;
    HoverFocusProcessor* const m_toolTipHoverProcessor;
    QScopedPointer<BusyIndicatorGraphicsWidget> m_busyIndicator;
    QPointer<QTimer> m_scheduledChangeTimer = nullptr;
    bool m_imagesDirty = false;
    bool m_updatingState = false;

    QElapsedTimer m_animationTime;
    QPixmap m_waitingPixmap;
    QPixmap m_successPixmap;
    QPixmap m_failurePixmap;
    QPixmap m_failureFramePixmap;
    QPixmap m_activityFramePixmap;
    QPixmap m_goToLivePixmap;
    QPixmap m_goToLivePixmapPressed;
};

} // namespace nx::vms::client::desktop
