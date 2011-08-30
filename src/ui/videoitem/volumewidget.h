#ifndef VOLUMEWIDGET_H
#define VOLUMEWIDGET_H

#include <QtGui/QSlider>

#include "navigationitem.h"

class MyButton;

class StyledSlider : public QSlider
{
    Q_OBJECT

public:
    StyledSlider(QWidget *parent = 0);
    StyledSlider(Qt::Orientation orientation, QWidget *parent = 0);
    ~StyledSlider();

protected:
    void paintEvent(QPaintEvent *);
    void timerEvent(QTimerEvent *);

    void sliderChange(SliderChange change);

private:
    int m_timerId;
};


class VolumeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VolumeWidget(QWidget *parent = 0);

    QSlider *slider() const { return m_slider; }

    QSize sizeHint() const { return QSize(50, 16); }

public Q_SLOTS:
    void onValueChanged(int);
    void onButtonChecked();

protected:
    bool eventFilter(QObject *, QEvent *);
    void paintEvent(QPaintEvent *);

private:
    StyledSlider *m_slider;
    MyButton *m_button;
};

#endif // VOLUMEWIDGET_H
