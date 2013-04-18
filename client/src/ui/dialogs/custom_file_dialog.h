#ifndef CUSTOM_FILE_DIALOG_H
#define CUSTOM_FILE_DIALOG_H

#include <QtCore/QMap>

#include <QtGui/QFileDialog>
#include <QtGui/QCheckBox>

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

    /**
     * @brief addCheckbox               Adds a checkbox to this file dialog.
     * 
     * @param text                      Checkbox text.
     * @param value                     Pointer to the initial value of the checkbox.
     *                                  This pointer will be saved and when the dialog
     *                                  closes, resulting checkbox value will be written into it.
     *                                  It is the callee's responsibility to make sure
     *                                  that pointed-to value still exists at that point.
     */
    void addCheckbox(const QString &text, bool *value);


    /**
     * @brief addWidget                 Adds custom widget to this file dialog.
     * @param widget                    Pointer to the widget.
     */
    void addWidget(QWidget* widget);
private slots:
    void at_accepted();
private:
    QMap<QCheckBox*, bool *> m_checkboxes;

};

#endif // CUSTOM_FILE_DIALOG_H
