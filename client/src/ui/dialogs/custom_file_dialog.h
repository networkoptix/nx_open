#ifndef CUSTOM_FILE_DIALOG_H
#define CUSTOM_FILE_DIALOG_H

#include <QFileDialog>

class QnCustomFileDialog : public QFileDialog
{
    Q_OBJECT

    typedef QFileDialog base_type;

public:
    explicit QnCustomFileDialog(QWidget *parent = 0,
                         const QString &caption = QString(),
                         const QString &directory = QString(),
                         const QString &filter = QString());
    ~QnCustomFileDialog();

    // TODO: #GDM this actually was a good idea so that callee doesn't mess with 
    // allocation of UI elements
    /**
     * Adds a checkbox to this file dialog.
     * 
     * \param text                      Checkbox text.
     * \param value                     Pointer to the initial value of the checkbox.
     *                                  This pointer will be saved and when the dialog
     *                                  closes, resulting checkbox value will be written into it.
     *                                  It is the callee's responsibility to make sure
     *                                  that pointed-to value still exists at that point.
     */
    void addCheckbox(const QString &text, bool *value);

    void addWidget(QWidget* widget);
};

#endif // CUSTOM_FILE_DIALOG_H
