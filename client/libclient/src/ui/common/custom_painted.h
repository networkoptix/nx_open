#pragma once

#include <functional>
#include <type_traits>

#include <QtGui/QPainter>

#include <QtWidgets/QStyleOption>
#include <QtWidgets/QWidget>

class CustomPaintedBase
{
public:
    using PaintFunction = std::function<bool (QPainter*, const QStyleOption*, const QWidget*)>;

public:
    PaintFunction customPaintFunction() const;
    void setCustomPaintFunction(PaintFunction function);

protected:
    virtual bool customPaint(QPainter* painter, const QStyleOption* option, const QWidget* widget);

private:
    PaintFunction m_paintFunction;
};

template<class Base>
class CustomPainted : public Base, public CustomPaintedBase
{
    static_assert(std::is_base_of<QWidget, Base>::value, "Base must be derived from QWidget");

public:
    CustomPainted(QWidget* parent = nullptr) : Base(parent) {}

    virtual void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        QStyleOption option;
        option.initFrom(this);
        option.rect = this->contentsRect();
        if (!customPaint(&painter, &option, this))
            Base::paintEvent(event);
    }
};
