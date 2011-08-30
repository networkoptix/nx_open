#ifndef VOLUMEWIDGET_H
#define VOLUMEWIDGET_H

#include <QtGui/QWidget>

class QSlider;

class MyButton;
class StyledSlider;

class VolumeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VolumeWidget(QWidget *parent = 0);

    QSlider *slider() const;

    QSize sizeHint() const;

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
