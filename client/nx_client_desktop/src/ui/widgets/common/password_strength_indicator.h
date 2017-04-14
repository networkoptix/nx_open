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
    Q_PROPERTY(QMargins indicatorMargins READ indicatorMargins WRITE setIndicatorMargins)
    Q_PROPERTY(qreal roundingRadius READ roundingRadius WRITE setRoundingRadius)
    Q_PROPERTY(int textMargins READ textMargins WRITE setTextMargins)

public:
    explicit QnPasswordStrengthIndicator(QLineEdit* lineEdit);
    virtual ~QnPasswordStrengthIndicator();

    const QnPasswordInformation& currentInformation() const;

    const QnPasswordStrengthColors& colors() const;
    void setColors(const QnPasswordStrengthColors& colors);

    const QMargins& indicatorMargins() const;
    void setIndicatorMargins(const QMargins& margins);

    qreal roundingRadius() const;
    void setRoundingRadius(qreal radius);

    int textMargins() const;
    void setTextMargins(int margins);

protected:
    virtual void paintEvent(QPaintEvent* event) override;

private:
    QScopedPointer<QnPasswordStrengthIndicatorPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnPasswordStrengthIndicator)
    Q_DISABLE_COPY(QnPasswordStrengthIndicator)
};
