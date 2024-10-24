// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <ui/graphics/items/generic/image_button_widget.h>

namespace nx::vms::client::desktop {

class CameraButton: public QnImageButtonWidget
{
    Q_OBJECT

    using base_type = QnImageButtonWidget;

public:
    enum class State
    {
        Default,
        WaitingForActivation,
        Activate,
        WaitingForDeactivation,
        Failure
    };

    CameraButton(QGraphicsItem* parent = nullptr,
        const std::optional<QColor>& pressedColor = {},
        const std::optional<QColor>& activeColor = {},
        const std::optional<QColor>& normalColor = {});

    virtual ~CameraButton() override;

    QString iconName() const;
    /* Set icon by name. Pixmap will be obtained from QnSoftwareTriggerIcons::pixmapByName. */
    void setIconName(const QString& name);

    State state() const;
    void setState(State state);

    bool isLive() const;
    void setLive(bool value);

    bool prolonged() const;
    void setProlonged(bool value);

    QSize buttonSize() const;
    void setButtonSize(const QSize& value);

signals:
    void stateChanged();
    void isLiveChanged();
    void prolongedChanged();

protected:
    using base_type::setIcon;

    QColor normalColor();
    QColor activeColor();
    QColor pressedColor() const;

    QPixmap generatePixmap(
        const QColor& background,
        const QColor& frame,
        const QPixmap& icon);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
