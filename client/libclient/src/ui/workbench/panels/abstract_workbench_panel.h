#pragma once

#include <ui/workbench/workbench_context_aware.h>

class AnimationTimer;

namespace NxUi {

class AbstractWorkbenchPanel: public QObject, public QnWorkbenchContextAware
{
    using base_type = QObject;

    Q_OBJECT
public:
    AbstractWorkbenchPanel(QObject* parent = nullptr);

    virtual bool isPinned() const = 0;

    virtual bool isOpened() const = 0;
    virtual void setOpened(bool opened = true, bool animate = true) = 0;

    virtual bool isVisible() const = 0;
    virtual void setVisible(bool visible = true, bool animate = true) = 0;

    virtual qreal opacity() const = 0;
    virtual void setOpacity(qreal opacity, bool animate = true) = 0;

    void updateOpacity(bool animate = true);
signals:
    void openedChanged(bool value);
    void visibleChanged(bool value);

protected:
    /** Make sure animation is allowed, set animate to false otherwise. */
    void ensureAnimationAllowed(bool* animate);

    AnimationTimer* animationTimer() const;
};

}