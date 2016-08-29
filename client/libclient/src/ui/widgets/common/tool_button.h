#pragma once

#include <QtWidgets/QToolButton>

class QnToolButton : public QToolButton
{
    Q_OBJECT

    typedef QToolButton base_type;

public:
    QnToolButton(QWidget* parent = nullptr);

    void adjustIconSize();

protected:
    virtual void mousePressEvent(QMouseEvent* event) override;

    virtual void showEvent(QShowEvent *event) override;

signals:
    void justPressed();
};
