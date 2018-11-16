#pragma once

#include <QtWidgets/QWidget>

#include <client/client_color_types.h>
#include <nx/vms/client/desktop/common/utils/password_information.h>

class QLineEdit;

namespace nx::vms::client::desktop {

class PasswordStrengthIndicator: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

    Q_PROPERTY(QnPasswordStrengthColors colors READ colors WRITE setColors)
    Q_PROPERTY(qreal roundingRadius READ roundingRadius WRITE setRoundingRadius)
    Q_PROPERTY(QMargins textMargins READ textMargins WRITE setTextMargins)

public:
    explicit PasswordStrengthIndicator(
        QLineEdit* lineEdit,
        PasswordInformation::AnalyzeFunction analyzeFunction);
    virtual ~PasswordStrengthIndicator() override;

    const PasswordInformation& currentInformation() const;

    const QnPasswordStrengthColors& colors() const;
    void setColors(const QnPasswordStrengthColors& colors);

    qreal roundingRadius() const;
    void setRoundingRadius(qreal radius);

    QMargins textMargins() const;
    void setTextMargins(const QMargins& value);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual QSize minimumSizeHint() const override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
