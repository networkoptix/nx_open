// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef PROGRESS_WIDGET_H
#define PROGRESS_WIDGET_H

#include <QtWidgets/QLabel>

class QnProgressWidget : public QWidget {
    Q_OBJECT

public:
    QnProgressWidget(QWidget *parent = 0);

    void setText(const QString &text);
private:
    QLabel* m_img;
    QLabel* m_text;
};

#endif // PROGRESS_WIDGET_H
