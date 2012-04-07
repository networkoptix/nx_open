#ifndef ABSTRACTGRAPHICSSLIDER_H
#define ABSTRACTGRAPHICSSLIDER_H

#include "graphicswidget.h"

class AbstractGraphicsSliderPrivate;
class AbstractGraphicsSlider : public QGraphicsWidget
{
    Q_OBJECT
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(int pageStep READ pageStep WRITE setPageStep)
    Q_PROPERTY(int sliderPosition READ sliderPosition WRITE setSliderPosition NOTIFY sliderMoved)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged USER true)
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

    int minimum() const;
    void setMinimum(int);

    int maximum() const;
    void setMaximum(int);

    void setRange(int min, int max);

    int singleStep() const;
    void setSingleStep(int);

    int pageStep() const;
    void setPageStep(int);

    bool hasTracking() const;
    void setTracking(bool enable);

    bool isSliderDown() const;
    void setSliderDown(bool);

    int sliderPosition() const;
    void setSliderPosition(int);

    int value() const;

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

public Q_SLOTS:
    void setValue(int);

Q_SIGNALS:
    void valueChanged(int value);

    void sliderPressed();
    void sliderMoved(int position);
    void sliderReleased();

    void rangeChanged(int min, int max);

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
