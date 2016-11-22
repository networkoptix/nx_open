#pragma once

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class AnimationTimer;
class QGraphicsWidget;
struct QnPaneSettings;

namespace NxUi {

class AbstractWorkbenchPanel: public Connective<QObject>, public QnWorkbenchContextAware
{
    using base_type = Connective<QObject>;

    Q_OBJECT
public:
    AbstractWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent = nullptr);

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

    /** This geometry the panel will have when all animations are finished. */
    virtual QRectF effectiveGeometry() const = 0;

    virtual void stopAnimations() = 0;

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

}