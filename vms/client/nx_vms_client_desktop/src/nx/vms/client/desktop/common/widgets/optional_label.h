#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QLabel>

namespace nx::vms::client::desktop {

/**
 * A label that displays a placeholder if its text is empty.
 */
class OptionalLabel: public QLabel
{
    Q_OBJECT
    using base_type = QLabel;

public:
    explicit OptionalLabel(QWidget* parent = nullptr);
    virtual ~OptionalLabel() override;

    QString placeholderText() const;
    void setPlaceholderText(const QString& value);

    QPalette::ColorRole placeholderRole() const;
    void setPlaceholderRole(QPalette::ColorRole value);

protected:
    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    virtual void paintEvent(QPaintEvent* event) override;

private:
    QLabel* const m_placeholderLabel;
};

} // namespace nx::vms::client::desktop
