#pragma once

#include <QtCore/QScopedPointer>

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
    class InputDialog;
}

/**
 * Text input dialog that will be automatically closed on client disconnect.
 * Also supports input validation and error text display.
 */
class QnInputDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    QnInputDialog(QWidget *parent = NULL);
    virtual ~QnInputDialog();

    QString caption() const;
    void setCaption(const QString &caption);

    QString value() const;
    void setValue(const QString &value);

    QString placeholderText() const;
    void setPlaceholderText(const QString &placeholderText);

    QDialogButtonBox::StandardButtons buttons() const;
    void setButtons(QDialogButtonBox::StandardButtons buttons);

private:
    void validateInput();

private:
    QScopedPointer<Ui::InputDialog> ui;
};
