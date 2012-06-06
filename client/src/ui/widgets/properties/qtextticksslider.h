#ifndef __TEXT_TICKS_SLIDER_H__
#define __TEXT_TICKS_SLIDER_H__

class QTextTicksSlider: public QSlider
{
public:
    QTextTicksSlider(QWidget* parent = 0);
    QTextTicksSlider(Qt::Orientation orientation, QWidget* parent = 0);
    virtual ~QTextTicksSlider();
protected:
    virtual void paintEvent(QPaintEvent * ev) override;
    virtual void resizeEvent(QResizeEvent * event) override;
};

#endif // __TEXT_TICKS_SLIDER_H__
