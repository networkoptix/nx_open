#pragma once

#include <QtWidgets/QScrollBar>

class QAbstractScrollArea;

class QnScrollBarProxy: public QScrollBar
{
    Q_OBJECT
    using base_type = QScrollBar;

public:
    QnScrollBarProxy(QWidget* parent = nullptr);
    QnScrollBarProxy(QScrollBar* scrollBar, QWidget* parent = nullptr);
    virtual ~QnScrollBarProxy();

    void setScrollBar(QScrollBar* scrollBar);

    virtual QSize sizeHint() const override;
    virtual bool event(QEvent* event) override;

    static void makeProxy(QScrollBar* scrollBar, QAbstractScrollArea* scrollArea);

protected:
    virtual void paintEvent(QPaintEvent*) override;
    virtual void sliderChange(SliderChange change) override;

private:
    void setScrollBarVisible(bool visible);

private:
    QScrollBar *m_scrollBar;
    bool m_visible;
};
