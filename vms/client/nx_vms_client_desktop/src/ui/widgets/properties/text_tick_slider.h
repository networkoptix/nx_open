// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_TEXT_TICK_SLIDER_H
#define QN_TEXT_TICK_SLIDER_H

#include <QtWidgets/QSlider>

class QnTextTickSlider: public QSlider {
    Q_OBJECT;

    typedef QSlider base_type;

public:
    QnTextTickSlider(QWidget *parent = 0);
    QnTextTickSlider(Qt::Orientation orientation, QWidget *parent = 0);
    virtual ~QnTextTickSlider();

protected:
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void changeEvent(QEvent *event) override;

    virtual QSize minimumSizeHint() const override;
};

#endif // QN_TEXT_TICK_SLIDER_H
