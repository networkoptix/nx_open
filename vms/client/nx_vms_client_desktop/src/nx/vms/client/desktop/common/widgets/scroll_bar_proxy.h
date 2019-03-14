#pragma once

#include <QtWidgets/QScrollBar>

class QAbstractScrollArea;

namespace nx::vms::client::desktop {

class ScrollBarProxy: public QScrollBar
{
    Q_OBJECT
    using base_type = QScrollBar;

public:
    explicit ScrollBarProxy(QWidget* parent = nullptr);
    explicit ScrollBarProxy(QScrollBar* scrollBar, QWidget* parent = nullptr);
    virtual ~ScrollBarProxy() override;

    void setScrollBar(QScrollBar* scrollBar);

    virtual QSize sizeHint() const override;
    virtual bool event(QEvent* event) override;

    static void makeProxy(QScrollBar* scrollBar, QAbstractScrollArea* scrollArea);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void sliderChange(SliderChange change) override;

private:
    void setScrollBarVisible(bool visible);

private:
    QScrollBar* m_scrollBar = nullptr;
    bool m_visible = true;
};

} // namespace nx::vms::client::desktop
