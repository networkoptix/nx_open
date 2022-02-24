// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>
#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/impl_ptr.h>
#include <ui/workbench/workbench_context_aware.h>

class QQuickWidget;
class QnWorkbenchContext;

namespace nx::vms::client::desktop {

/**
 * Tooltip as a QQuickWidget with Nx.Controls.Bubble or its descendant as a content.
 */
class BubbleToolTip:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    BubbleToolTip(QnWorkbenchContext* context, QObject* parent = nullptr);
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
    BubbleToolTip(QnWorkbenchContext* context, const QUrl& componentUrl, QObject* parent = nullptr);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
