#ifndef __TEXT_TICKS_SLIDER_H__
#define __TEXT_TICKS_SLIDER_H__

#include <QtGui/QSlider>

class QTextTicksSlider: public QSlider {
    Q_OBJECT;

    typedef QSlider base_type;

public:
    QTextTicksSlider(QWidget *parent = 0);
    QTextTicksSlider(Qt::Orientation orientation, QWidget *parent = 0);
    virtual ~QTextTicksSlider();

protected:
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void changeEvent(QEvent *event) override;

    virtual QSize minimumSizeHint() const override;
};

#endif // __TEXT_TICKS_SLIDER_H__
