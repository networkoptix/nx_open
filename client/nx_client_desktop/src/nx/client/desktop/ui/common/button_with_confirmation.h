#pragma once

#include <chrono>

#include <QtWidgets/QPushButton>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

// A flat button that switches into non-interactive confirmation state with different text
// and icon for a short duration after being clicked, then animates back to normal state.
class ButtonWithConfirmation: public QPushButton
{
    Q_OBJECT
    using base_type = QPushButton;

public:
    explicit ButtonWithConfirmation(QWidget* parent = nullptr);
    explicit ButtonWithConfirmation(
        const QIcon& mainIcon,
        const QString& mainText,
        const QIcon& confirmationIcon,
        const QString& confirmationText,
        QWidget* parent = nullptr);

    virtual ~ButtonWithConfirmation() override;

    QString mainText() const;
    void setMainText(const QString& value);

    QString confirmationText() const;
    void setConfirmationText(const QString& value);

    QIcon mainIcon() const;
    void setMainIcon(const QIcon& value);

    QIcon confirmationIcon() const;
    void setConfirmationIcon(const QIcon& value);

    std::chrono::milliseconds confirmationDuration() const;
    void setConfirmationDuration(std::chrono::milliseconds value);

    std::chrono::milliseconds fadeOutDuration() const;
    void setFadeOutDuration(std::chrono::milliseconds value);

    std::chrono::milliseconds fadeInDuration() const;
    void setFadeInDuration(std::chrono::milliseconds value);

protected:
    virtual bool event(QEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
