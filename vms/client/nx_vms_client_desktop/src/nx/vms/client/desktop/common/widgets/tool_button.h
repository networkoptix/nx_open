#pragma once

#include <QtWidgets/QToolButton>

namespace nx::vms::client::desktop {

class ToolButton: public QToolButton
{
    Q_OBJECT
    using base_type = QToolButton;

public:
    ToolButton(QWidget* parent = nullptr);
    void adjustIconSize();

    enum Background
    {
        NormalBackground = 0x1,
        HoveredBackground = 0x2,
        PressedBackground = 0x4,
        ActiveBackgrounds = HoveredBackground | PressedBackground,
        AllBackgrounds = NormalBackground | HoveredBackground | PressedBackground
    };
    Q_DECLARE_FLAGS(Backgrounds, Background);

    Backgrounds drawnBackgrounds() const;
    void setDrawnBackgrounds(Backgrounds value);

protected:
    virtual QSize calculateIconSize() const;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;

signals:
    void justPressed();

private:
    Backgrounds m_drawnBackgrounds = AllBackgrounds;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ToolButton::Backgrounds);

} // namespace nx::vms::client::desktop
