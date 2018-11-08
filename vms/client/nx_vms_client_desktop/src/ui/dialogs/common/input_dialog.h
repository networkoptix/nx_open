#pragma once

#include <QtCore/QScopedPointer>

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLineEdit>

#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/vms/client/desktop/common/widgets/input_field.h>

namespace Ui {
class InputDialog;
} // namespace Ui

/**
 * Text input dialog that will be automatically closed on client disconnect.
 * Also supports input validation and error text display.
 */
class QnInputDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    QnInputDialog(QWidget* parent);
    virtual ~QnInputDialog() override;

    QString caption() const;
    void setCaption(const QString& caption);

    QString value() const;
    void setValue(const QString& value);

    void setValidator(nx::vms::client::desktop::TextValidateFunction validator);

    QString placeholderText() const;
    void setPlaceholderText(const QString& placeholderText);

    QLineEdit::EchoMode echoMode() const;
    void setEchoMode(QLineEdit::EchoMode echoMode);

    QDialogButtonBox::StandardButtons buttons() const;
    void setButtons(QDialogButtonBox::StandardButtons buttons);

    void useForPassword();

    static QString getText(QWidget* parent,
        const QString& title, const QString& label,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        QLineEdit::EchoMode echoMode = QLineEdit::Normal,
        const QString& placeholder = QString(),
        const QString& initialText = QString());

private:
    virtual void accept() override;

private:
    QScopedPointer<Ui::InputDialog> ui;
};
