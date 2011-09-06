#ifndef SPEEDWIDGET_H
#define SPEEDWIDGET_H

#include <QtGui/QSlider>
#include <QtGui/QWidget>

class QPropertyAnimation;
class QToolButton;

class SpeedSlider : public QSlider
{
    Q_OBJECT

public:
    explicit SpeedSlider(QWidget *parent = 0);
    ~SpeedSlider();

Q_SIGNALS:
    void speedChanged(float newSpeed);

public Q_SLOTS:
    void resetSpeed();

protected:
    void paintEvent(QPaintEvent *ev);
    void mouseReleaseEvent(QMouseEvent *ev);

    void sliderChange(SliderChange change);

private:
    QPropertyAnimation *m_animation;
};


class SpeedWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpeedWidget(QWidget *parent = 0);

    QSize sizeHint() const;

public Q_SLOTS:
    void resetSpeed();

Q_SIGNALS:
    void speedChanged(float newSpeed);

protected:
    bool eventFilter(QObject *, QEvent *);
    void paintEvent(QPaintEvent *);

private Q_SLOTS:
    void onButtonClicked();

private:
    SpeedSlider *m_slider;
    QToolButton *m_leftButton;
    QToolButton *m_rightButton;
};

#endif // SPEEDWIDGET_H
