#pragma once

#include <QtWidgets/QScrollBar>

class QAbstractScrollArea;

class QnScrollBarProxy : public QScrollBar
{
    typedef QScrollBar base_type;

public:
    QnScrollBarProxy(QWidget *parent = nullptr);
    QnScrollBarProxy(QScrollBar *scrollBar, QWidget *parent = nullptr);
    ~QnScrollBarProxy();

    void setScrollBar(QScrollBar *scrollBar);

    QSize sizeHint() const override;

    bool event(QEvent *event) override;

    static void makeProxy(QScrollBar *scrollBar, QAbstractScrollArea *scrollArea);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void setScrollBarVisible(bool visible);

private:
    QScrollBar *m_scrollBar;
    bool m_visible;
};
