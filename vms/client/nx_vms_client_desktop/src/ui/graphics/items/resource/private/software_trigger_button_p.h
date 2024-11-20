// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QtCore/QTimer>

#include "../software_trigger_button.h"

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

    void updateState();

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget);

    void scheduleChange(std::function<void()> callback, int delayMs);
    void cancelScheduledChange();

    void ensureImages();

private:
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
};

} // namespace nx::vms::client::desktop
