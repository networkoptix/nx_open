// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>

#include "../jump_to_live_button.h"

class HoverFocusProcessor;
class QnStyledTooltipWidget;
class QTimer;

namespace nx::vms::client::desktop {

class JumpToLiveButton;

class JumpToLiveButtonPrivate: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_DECLARE_PUBLIC(JumpToLiveButton)
    JumpToLiveButton* const q_ptr;

public:
    JumpToLiveButtonPrivate(JumpToLiveButton* main);
    virtual ~JumpToLiveButtonPrivate();

    QString toolTip() const;
    void setToolTip(const QString& toolTip);

    Qt::Edge toolTipEdge() const;
    void setToolTipEdge(Qt::Edge edge);

    void updateState();

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget);

    void updateToolTipText();

private:
    void updateToolTipPosition();
    void updateToolTipTailEdge();
    void updateToolTipVisibility();

    void ensureImages();

private:
    Qt::Edge m_toolTipEdge = Qt::LeftEdge;
    QString m_toolTipText;

    QnStyledTooltipWidget* const m_toolTip;
    HoverFocusProcessor* const m_toolTipHoverProcessor;
    QPointer<QTimer> m_scheduledChangeTimer = nullptr;
    bool m_imagesDirty = false;
    bool m_updatingState = false;

    QElapsedTimer m_animationTime;
    QPixmap m_goToLivePixmap;
    QPixmap m_goToLivePixmapPressed;
};

} // namespace nx::vms::client::desktop
