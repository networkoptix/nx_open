#pragma once

#include <ui/workbench/workbench_context_aware.h>

class AnimationTimer;
class QGraphicsWidget;
struct QnPaneSettings;

namespace NxUi {

class AbstractWorkbenchPanel: public QObject, public QnWorkbenchContextAware
{
    using base_type = QObject;

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

    virtual qreal opacity() const = 0;
    virtual void setOpacity(qreal opacity, bool animate = true) = 0;
    void updateOpacity(bool animate = true);

    virtual bool isHovered() const = 0;

signals:
    void openedChanged(bool value);
    void visibleChanged(bool value);

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
    QRectF m_parentWidgetRect;
};

}