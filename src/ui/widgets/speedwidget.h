#ifndef SPEEDWIDGET_H
#define SPEEDWIDGET_H

#include <QtGui/QWidget>

class QSlider;

class StyledSlider;

class SpeedWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpeedWidget(QWidget *parent = 0);

    QSlider *slider() const;

    QSize sizeHint() const;

public Q_SLOTS:
    void onValueChanged(int);

protected:
    bool eventFilter(QObject *, QEvent *);
    void paintEvent(QPaintEvent *);

private:
    StyledSlider *m_slider;
};

#endif // SPEEDWIDGET_H
