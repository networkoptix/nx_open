#ifndef SPEEDWIDGET_H
#define SPEEDWIDGET_H

#include <QtGui/QWidget>

class QSlider;
class QToolButton;

class StyledSlider;

class SpeedWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpeedWidget(QWidget *parent = 0);

    QSlider *slider() const;

    QSize sizeHint() const;

Q_SIGNALS:
    void speedChanged(float newSpeed);

protected:
    bool eventFilter(QObject *, QEvent *);
    void paintEvent(QPaintEvent *);

public Q_SLOTS:
    void onValueChanged(int);
    void onButtonClicked();

private:
    StyledSlider *m_slider;
    QToolButton *m_leftButton;
    QToolButton *m_rightButton;
};

#endif // SPEEDWIDGET_H
