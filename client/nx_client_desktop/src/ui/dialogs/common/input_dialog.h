#pragma once

#include <QtCore/QScopedPointer>

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/common/session_aware_dialog.h>

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
    QnInputDialog(QWidget* parent = nullptr);
    virtual ~QnInputDialog() override;

    QString caption() const;
    void setCaption(const QString& caption);

    QString value() const;
    void setValue(const QString& value);

    QString placeholderText() const;
    void setPlaceholderText(const QString& placeholderText);

    QDialogButtonBox::StandardButtons buttons() const;
    void setButtons(QDialogButtonBox::StandardButtons buttons);

    static QString getText(QWidget* parent,
        const QString& title, const QString& label,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        const QString& placeholder = QString(),
        const QString& initialText = QString());

private:
    void validateInput();

private:
    QScopedPointer<Ui::InputDialog> ui;
};
