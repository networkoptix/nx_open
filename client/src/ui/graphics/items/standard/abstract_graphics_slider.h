#ifndef ABSTRACTGRAPHICSSLIDER_H
#define ABSTRACTGRAPHICSSLIDER_H

#include "graphics_widget.h"

class AbstractGraphicsSliderPrivate;
class AbstractGraphicsSlider : public GraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(qint64 minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(qint64 maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(qint64 singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(qint64 pageStep READ pageStep WRITE setPageStep)
    Q_PROPERTY(qint64 sliderPosition READ sliderPosition WRITE setSliderPosition NOTIFY sliderMoved)
    Q_PROPERTY(qint64 value READ value WRITE setValue NOTIFY valueChanged USER true)
    Q_PROPERTY(bool tracking READ hasTracking WRITE setTracking)
    Q_PROPERTY(bool invertedAppearance READ invertedAppearance WRITE setInvertedAppearance)
    Q_PROPERTY(bool invertedControls READ invertedControls WRITE setInvertedControls)
    Q_PROPERTY(bool sliderDown READ isSliderDown WRITE setSliderDown DESIGNABLE false)
    Q_PROPERTY(bool acceleratedWheeling READ isWheelingAccelerated WRITE setWheelingAccelerated)

public:
    explicit AbstractGraphicsSlider(QGraphicsItem *parent = 0);
    virtual ~AbstractGraphicsSlider();

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation);

    qint64 minimum() const;
    void setMinimum(qint64);

    qint64 maximum() const;
    void setMaximum(qint64);

    void setRange(qint64 min, qint64 max);

    qint64 singleStep() const;
    void setSingleStep(qint64);

    qint64 pageStep() const;
    void setPageStep(qint64);

    bool hasTracking() const;
    void setTracking(bool enable);

    bool isSliderDown() const;
    void setSliderDown(bool);

    qint64 sliderPosition() const;
    void setSliderPosition(qint64);

    qint64 value() const;

    bool invertedAppearance() const;
    void setInvertedAppearance(bool);

    bool invertedControls() const;
    void setInvertedControls(bool);

    bool isWheelingAccelerated() const;
    void setWheelingAccelerated(bool);

    enum SliderAction {
        SliderNoAction,
        SliderSingleStepAdd,
        SliderSingleStepSub,
        SliderPageStepAdd,
        SliderPageStepSub,
        SliderToMinimum,
        SliderToMaximum,
        SliderMove
    };

    void triggerAction(SliderAction action);

    virtual QPointF positionFromValue(qint64 logicalValue) const = 0;
    virtual qint64 valueFromPosition(const QPointF &position) const = 0;

public Q_SLOTS:
    void setValue(qint64);

Q_SIGNALS:
    void valueChanged(qint64 value);

    void sliderPressed();
    void sliderMoved(qint64 position);
    void sliderReleased();

    void rangeChanged(qint64 min, qint64 max);

    void actionTriggered(int action);

protected:
    SliderAction repeatAction() const;
    void setRepeatAction(SliderAction action, int thresholdTime = 500, int repeatTime = 50);

    enum SliderChange {
        SliderRangeChange,
        SliderOrientationChange,
        SliderStepsChange,
        SliderValueChange
    };
    virtual void sliderChange(SliderChange change);

    void initStyleOption(QStyleOption *option) const;

    bool event(QEvent *event);
    void changeEvent(QEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void timerEvent(QTimerEvent *event);
#ifndef QT_NO_WHEELEVENT
    void wheelEvent(QGraphicsSceneWheelEvent *event);
#endif

protected:
    AbstractGraphicsSlider(AbstractGraphicsSliderPrivate &dd, QGraphicsItem *parent);

private:
    Q_DISABLE_COPY(AbstractGraphicsSlider)
    Q_DECLARE_PRIVATE(AbstractGraphicsSlider)
};

#endif // ABSTRACTGRAPHICSSLIDER_H
