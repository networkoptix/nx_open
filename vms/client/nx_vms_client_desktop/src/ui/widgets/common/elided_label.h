// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef ELIDED_LABEL_H
#define ELIDED_LABEL_H

#include <QtWidgets/QLabel>

class QnElidedLabelPrivate;

class QnElidedLabel : public QLabel {
    Q_OBJECT

public:
    QnElidedLabel(QWidget *parent = 0);

    Qt::TextElideMode elideMode() const;
    void setElideMode(Qt::TextElideMode elideMode);

protected:
    virtual void paintEvent(QPaintEvent *event) override;

private:
    Qt::TextElideMode m_elideMode;
};

#endif // ELIDED_LABEL_H
