#ifndef STYLEDSLIDER_H
#define STYLEDSLIDER_H

#include <QtGui/QSlider>

class StyledSlider : public QSlider
{
    Q_OBJECT

public:
    explicit StyledSlider(QWidget *parent = 0);
    explicit StyledSlider(Qt::Orientation orientation, QWidget *parent = 0);
    ~StyledSlider();

    QString valueTex() const;
    void setValueText(const QString &text);

protected:
    void paintEvent(QPaintEvent *);
    void timerEvent(QTimerEvent *);

    void sliderChange(SliderChange change);

private:
    int m_timerId;
    QString m_text;
};

#endif // STYLEDSLIDER_H
