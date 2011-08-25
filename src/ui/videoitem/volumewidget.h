#ifndef VOLUMEWIDGET_H
#define VOLUMEWIDGET_H

#include <QtGui/QSlider>

#include "navigationitem.h"

class MyButton;

class MyVolumeSlider : public QSlider
{
    Q_OBJECT

public:
    MyVolumeSlider(QWidget *parent = 0) : QSlider(Qt::Horizontal, parent) {}
    MyVolumeSlider(Qt::Orientation orientation, QWidget *parent = 0) : QSlider(orientation, parent) {}

    void paintEvent(QPaintEvent *ev);
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
    MyVolumeSlider *m_slider;
    MyButton *m_button;
};

#endif // VOLUMEWIDGET_H
