#ifndef VOLUMEWIDGET_H
#define VOLUMEWIDGET_H

#include <QWidget>
#include <QSlider>
#include "navigationitem.h"

class MyButton;
class MyVolumeSlider : public QSlider
{
public:
    MyVolumeSlider(QWidget *parent = 0) : QSlider(Qt::Horizontal, parent) {}
    void paintEvent(QPaintEvent *ev);
};

class VolumeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VolumeWidget(QWidget *parent = 0);

    QSlider *slider() const { return m_slider; }
    QSize sizeHint() const { return QSize(50, 16); }
    void paintEvent(QPaintEvent *);

signals:

public slots:
    void onValueChanged(int);
    void onButtonChecked();

private:
    MyVolumeSlider *m_slider;
    MyButton *m_button;
};

#endif // VOLUMEWIDGET_H
