#pragma once

#include <QtGui/QPixmap>
#include <QtWidgets/QWidget>

class QVariantAnimation;
class QSequentialAnimationGroup;

namespace nx::vms::client::desktop {

/**
 * An utility widget that animates transition between two pixmaps.
 */
class TransitionerWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit TransitionerWidget(QWidget* parent = nullptr);
    virtual ~TransitionerWidget() override;

    void setSource(const QPixmap& pixmap);
    void setTarget(const QPixmap& pixmap);

    int fadeOutDurationMs() const;
    void setFadeOutDurationMs(int value);

    int fadeInDurationMs() const;
    void setFadeInDurationMs(int value) const;

    void start();
    bool running() const;

signals:
    void finished();

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    void setOpacity(qreal value);
    QVariantAnimation* createAnimation();

private:
    qreal m_opacity = 1.0;
    QPixmap m_source, m_target, m_current;
    QVariantAnimation* const m_fadeOut = nullptr;
    QVariantAnimation* const m_fadeIn = nullptr;
    QSequentialAnimationGroup* const m_animationGroup = nullptr;
};

} // namespace nx::vms::client::desktop
