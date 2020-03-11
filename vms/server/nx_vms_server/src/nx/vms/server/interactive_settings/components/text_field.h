#pragma once

#include "string_value_item.h"

namespace nx::vms::server::interactive_settings::components {

class TextField: public StringValueItem
{
    Q_OBJECT
    Q_PROPERTY(QString validationRegex READ validationRegex WRITE setValidationRegex
        NOTIFY validationRegexChanged)
    Q_PROPERTY(QString validationRegexFlags READ validationRegexFlags WRITE setValidationRegexFlags
        NOTIFY validationRegexFlagsChanged)
    Q_PROPERTY(QString validationErrorMessage READ validationErrorMessage
        WRITE setValidationErrorMessage NOTIFY validationErrorMessageChanged)

public:
    TextField(QObject* parent = nullptr);

    /**
     * Regular expression pattern to check the value.
     * Check failure does not avoid server from saving wrong values. This regex is just for UI to
     * notify a user about wrong input.
     */
    QString validationRegex() const;

    void setValidationRegex(const QString& regex);

    /**
     * Javascript RegExp flags.
     * See RegExp documentation for details.
     */
    QString validationRegexFlags() const;

    void setValidationRegexFlags(const QString& flags);

    /**
     * When the item value does not satisfy the validation regex, this message should be displayed
     * under the input box.
     */
    QString validationErrorMessage() const;

    void setValidationErrorMessage(const QString& message);

    virtual QJsonObject serializeModel() const override;

signals:
    void validationRegexChanged();
    void validationRegexFlagsChanged();
    void validationErrorMessageChanged();

private:
    QString m_validationRegex;
    QString m_validationRegexFlags;
    QString m_validationErrorMessage;
};


} // namespace nx::vms::server::interactive_settings::components
