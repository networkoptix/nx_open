#pragma once

#include <QtCore/QScopedPointer>

#include <ui/graphics/items/generic/image_button_widget.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace graphics {

class SoftwareTriggerButtonPrivate;

class SoftwareTriggerButton: public QnImageButtonWidget
{
    Q_OBJECT
    using base_type = QnImageButtonWidget;

public:
    SoftwareTriggerButton(QGraphicsItem* parent = nullptr);
    virtual ~SoftwareTriggerButton();

    QString toolTip() const;
    void setToolTip(const QString& toolTip);

    Qt::Edge toolTipEdge() const;
    void setToolTipEdge(Qt::Edge edge);

    QSize buttonSize() const;
    void setButtonSize(const QSize& size);

    /* Set icon by name. Pixmap will be obtained from QnSoftwareTriggerIcons::pixmapByName. */
    void setIcon(const QString& name);

    bool prolonged() const;
    void setProlonged(bool value);

    enum class State
    {
        Default, //< default state
        Waiting, //< waiting for server response state
        Success, //< show success notification state
        Failure  //< show failure notification state
    };

    State state() const;
    void setState(State state);

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    bool isLive() const;
    void setLive(bool value);

signals:
    void isLiveChanged();

protected:
    using base_type::setIcon;

private:
    Q_DECLARE_PRIVATE(SoftwareTriggerButton)
    QScopedPointer<SoftwareTriggerButtonPrivate> d_ptr;
};

} // namespace graphics
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
