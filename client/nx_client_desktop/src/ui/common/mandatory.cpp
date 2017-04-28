#include "mandatory.h"

#include <ui/style/custom_style.h>

bool defaultValidateFunction( const QString &text ) {
    return !text.trimmed().isEmpty();
}

void declareMandatoryField( QLabel* label, QLineEdit* lineEdit /* = nullptr*/, ValidateFunction isValid /* = defaultValidateFunction*/ ) {
    if (!lineEdit)
        lineEdit = qobject_cast<QLineEdit*>(label->buddy());

    NX_ASSERT(label && lineEdit, Q_FUNC_INFO, "Widgets must be set here");
    if (!label || !lineEdit)
        return;

    /* On every field text change we should check its contents and repaint label with corresponding color. */
    QObject::connect(lineEdit, &QLineEdit::textChanged, label, [label, isValid](const QString &text) {
        if (!label->parentWidget())
            return;

        QPalette palette = label->parentWidget()->palette();
        if (!isValid(text))
            setWarningStyle(&palette);
        label->setPalette(palette);
    });

    if (!isValid(lineEdit->text()))
        setWarningStyle(label);
}
