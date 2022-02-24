// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

/**
 * Allows to imitate QWidgte::setAttribute(Qt::WA_TransparentForMouseEvents) effect.
 */
class UnbodiedGraphicsProxyWidget: public QGraphicsProxyWidget
{
    Q_OBJECT
    using base_type = QGraphicsProxyWidget;

public:
    using base_type::base_type;

    /**
     * Imitates QWidgte::setAttribute(Qt::WA_TransparentForMouseEvents) effect if true passed.
     * May have negative effects on item repainting.
     */
    void setTransparentForMouseEvents(bool transparent);

    /**
     * @return Empty QPainterPath if true passed into setTransparentForMouseEvents().
     *     This is the way to force item to transfer hoverMoveEvent() and hoverLeaveEvent()
     *     to parent item.
     */
    virtual QPainterPath shape() const override;

private:
    bool m_transparent = false;
};

/**
 * Encapsulates workarounds required to get full functionality from QWidget subclass wrapped
 * by QGraphicsProxyWidget. If regular QGraphicsProxyWidget creation and usage does not lose
 * any functionality of your QWidget, don't use this class.
 * Base class must be QWidget subclass.
 * Base class constructor may have any parameters, but pointer to parent QWidget must be the last.
 */
template<class Base>
class GraphicsProxyWrapped: public Base
{
public:

    /**
     * @param parentItem Parent QGraphicsItem for new proxy item that will be created.
     * @param args Parameters to pass to Base class constructor except trailing QWidget*.
     */
    template<typename... Args>
    GraphicsProxyWrapped(
        QGraphicsItem* parentItem,
        Args... args)
        :
        // QWidget's parent must be nullptr to get QGraphicsProxyWidget working at all.
        Base(args..., /*parent*/ static_cast<QWidget*>(nullptr)),
        m_proxy(new UnbodiedGraphicsProxyWidget(parentItem))
    {
        // This is the only way to get transparent background. If any subsequent
        // QWidget::setStyleSheet() calls are required (which would effectively cancel this
        // workaround), then more complex logic for this workaround should be implemented.
        this->setStyleSheet("background-color: transparent");

        m_proxy->setWidget(this);
    }

    UnbodiedGraphicsProxyWidget* proxyItem()
    {
        return m_proxy;
    }

private:
    QPointer<UnbodiedGraphicsProxyWidget> m_proxy;
};

} // namespace nx::vms::client::desktop
