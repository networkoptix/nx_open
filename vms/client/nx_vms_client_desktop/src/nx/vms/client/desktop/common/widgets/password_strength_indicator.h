// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/common/utils/password_information.h>

class QLineEdit;

namespace nx::vms::client::desktop {

class PasswordStrengthIndicator: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

    Q_PROPERTY(qreal roundingRadius READ roundingRadius WRITE setRoundingRadius)
    Q_PROPERTY(QMargins textMargins READ textMargins WRITE setTextMargins)

public:
    explicit PasswordStrengthIndicator(
        QLineEdit* lineEdit,
        PasswordInformation::AnalyzeFunction analyzeFunction);
    virtual ~PasswordStrengthIndicator() override;

    const PasswordInformation& currentInformation() const;

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
