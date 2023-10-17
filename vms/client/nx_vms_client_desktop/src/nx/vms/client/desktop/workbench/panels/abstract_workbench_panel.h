// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/window_context_aware.h>

class AnimationTimer;
class QGraphicsWidget;
struct QnPaneSettings;

namespace nx::vms::client::desktop {

class AbstractWorkbenchPanel: public QObject, public WindowContextAware
{
    using base_type = QObject;

    Q_OBJECT
public:
    AbstractWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent);

    virtual bool isPinned() const = 0;

    virtual bool isOpened() const = 0;
    virtual void setOpened(bool opened = true, bool animate = true) = 0;

    virtual bool isVisible() const = 0;
    virtual void setVisible(bool visible = true, bool animate = true) = 0;

    /* This is visibility-related virtual opacity in range [0.0, 1.0]. */
    virtual qreal opacity() const = 0;
    virtual void setOpacity(qreal opacity, bool animate = true) = 0;
    virtual void updateOpacity(bool animate = true);

    virtual bool isHovered() const = 0;
    virtual bool isFocused() const { return false; }

    /** This geometry the panel will have when all animations are finished. */
    virtual QRectF effectiveGeometry() const = 0;

    /** Current panel geometry. */
    virtual QRectF geometry() const = 0;

    virtual void stopAnimations() = 0;
    /** Adjust panel size. */
    virtual void setPanelSize(qreal size) = 0;

    /** Master opacity. Descendants should multiply visibility-related opacity by
      * the master opacity to obtain final effective opacity for graphics items. */
    qreal masterOpacity() const;
    void setMasterOpacity(qreal value);

signals:
    void openedChanged(bool value, bool animated);
    void visibleChanged(bool value, bool animated);

    void hoverEntered();
    void hoverLeft();

    void geometryChanged();

protected:
    /** Make sure animation is allowed, set animate to false otherwise. */
    void ensureAnimationAllowed(bool* animate);

    AnimationTimer* animationTimer() const;

    virtual void setProxyUpdatesEnabled(bool updatesEnabled);

protected:
    const QGraphicsWidget* m_parentWidget;
    qreal m_masterOpacity;
};

} // namespace nx::vms::client::desktop
