#pragma once

#include <ui/workbench/panels/abstract_workbench_panel.h>
#include <utils/common/connective.h>

class QnImageButtonWidget;
class HoverFocusProcessor;
class AnimatorGroup;
class VariantAnimator;
class QGraphicsProxyWidget;

namespace NxUi {

class TitleWorkbenchPanel: public Connective<AbstractWorkbenchPanel>
{
    using base_type = Connective<AbstractWorkbenchPanel>;

    Q_OBJECT
public:
    TitleWorkbenchPanel(
        const QnPaneSettings& settings,
        QGraphicsWidget* parentWidget,
        QObject* parent = nullptr);

    /** Title bar widget. */
    QGraphicsProxyWidget* item;

public:
    virtual bool isPinned() const override;

    virtual bool isOpened() const override;
    virtual void setOpened(bool opened = true, bool animate = true) override;

    virtual bool isVisible() const override;
    virtual void setVisible(bool visible = true, bool animate = true) override;

    virtual qreal opacity() const override;
    virtual void setOpacity(qreal opacity, bool animate = true) override;

    virtual bool isHovered() const override;

    virtual void stopAnimations() override;

    bool isUsed() const;
    void setUsed(bool value);

    virtual QRectF effectiveGeometry() const override;

private:
    void updateControlsGeometry();

private:
    bool m_ignoreClickEvent = false;
    bool m_visible = false;
    bool m_used = false;

    QPointer<QnImageButtonWidget> m_showButton;

    QPointer<AnimatorGroup> m_opacityAnimatorGroup;

    /** Animator for title's position. */
    QPointer<VariantAnimator> m_yAnimator;

    QPointer<HoverFocusProcessor> m_opacityProcessor;
};

} //namespace NxUi
