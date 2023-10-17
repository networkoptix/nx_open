// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QPoint>
#include <QtCore/QPointer>
#include <QtCore/QRect>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/window_context_aware.h>

class QQuickWidget;
class QnWorkbenchContext;

namespace nx::vms::client::desktop {

/**
 * Tooltip as a QQuickWidget with Nx.Controls.Bubble or its descendant as a content.
 */
class BubbleToolTip:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT

public:
    BubbleToolTip(WindowContext* context, QObject* parent = nullptr);
    virtual ~BubbleToolTip() override;

    void show();
    void hide(bool immediately = false);
    void suppress(bool immediately = false);

    QString text() const;
    void setText(const QString& value);

    Qt::Orientation orientation() const; //< Default Qt::Horizontal.
    void setOrientation(Qt::Orientation value);

    bool suppressedOnMouseClick() const; //< Default true.
    void setSuppressedOnMouseClick(bool value);

    /**
     * @return Distance between target point and the tip of tooltip pointer in device independent
     *     pixels. Tooltip offset is zero by default.
     */
    int tooltipOffset() const;

    /**
     * Sets distance between target point and the tip of the tooltip pointer. Offset may be applied
     * to avoid overlapping between tooltip and GUI controls.
     * @param offset Distance in device independent pixels.
     */
    void setTooltipOffset(int offset);

    // Target and enclosing rect are specified in global coordinates.
    void setTarget(const QRect& targetRect);
    void setTarget(const QPoint& targetPoint);
    void setEnclosingRect(const QRect& value);

    enum class State
    {
        shown,
        hidden,
        suppressed
    };

    State state() const;

    QQuickWidget* widget() const;

signals:
    void stateChanged(State state);

protected:
    BubbleToolTip(WindowContext* context, const QUrl& componentUrl, QObject* parent = nullptr);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
