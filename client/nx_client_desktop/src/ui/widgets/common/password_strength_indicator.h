#pragma once

#include <QtWidgets/QWidget>

#include <client/client_color_types.h>
#include <ui/utils/password_information.h>

class QLineEdit;
class QnPasswordStrengthIndicatorPrivate;

class QnPasswordStrengthIndicator : public QWidget
{
    Q_OBJECT
    typedef QWidget base_type;

    Q_PROPERTY(QnPasswordStrengthColors colors READ colors WRITE setColors)
    Q_PROPERTY(qreal roundingRadius READ roundingRadius WRITE setRoundingRadius)
    Q_PROPERTY(QMargins textMargins READ textMargins WRITE setTextMargins)

public:
    explicit QnPasswordStrengthIndicator(
        QLineEdit* lineEdit,
        QnPasswordInformation::AnalyzeFunction analyzeFunction);
    virtual ~QnPasswordStrengthIndicator() override;

    const QnPasswordInformation& currentInformation() const;

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
    QScopedPointer<QnPasswordStrengthIndicatorPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnPasswordStrengthIndicator)
    Q_DISABLE_COPY(QnPasswordStrengthIndicator)
};
