// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>
#include <QtCore/QVariant>
#include <QtWidgets/QWidget>
#include <QtWidgets/QGraphicsOpacityEffect>

class QEvent;

const char* opacityPropertyName();

class WidgetOpacityMixinEventFilter : public QObject
{
    Q_OBJECT

public:
    bool eventFilter(QObject* obj, QEvent* event);

signals:
    void opacityChanged();
};

/**
 * Encapsulate widget opacity behavior.
 * It must be QWidget inheritor.
 */

template <typename BaseClass>
class WidgetOpacityMixin : public BaseClass
{
public:
    using BaseClass::BaseClass;

    virtual ~WidgetOpacityMixin() = default;

    void initOpacityMixin();

    qreal opacity() const;
    void setOpacity(qreal value);

private:
    QGraphicsOpacityEffect* m_opacityEffect;
    WidgetOpacityMixinEventFilter m_eventFilter;
};

template<typename BaseClass>
void WidgetOpacityMixin<BaseClass>::initOpacityMixin()
{
    m_opacityEffect = new QGraphicsOpacityEffect(this);

    BaseClass::setGraphicsEffect(m_opacityEffect);

    BaseClass::installEventFilter(&m_eventFilter);
    BaseClass::connect(&m_eventFilter, &WidgetOpacityMixinEventFilter::opacityChanged, this,
        [this]
        {
            m_opacityEffect->setOpacity(opacity());
            BaseClass::setVisible(opacity() > 0);
        });

    BaseClass::setProperty(opacityPropertyName(), QVariant(0.0));
}

template<typename BaseClass>
qreal WidgetOpacityMixin<BaseClass>::opacity() const
{
    return BaseClass::property(opacityPropertyName()).toReal();
}

template<typename BaseClass>
void WidgetOpacityMixin<BaseClass>::setOpacity(qreal value)
{
    BaseClass::setProperty(opacityPropertyName(), QVariant(value));
}
