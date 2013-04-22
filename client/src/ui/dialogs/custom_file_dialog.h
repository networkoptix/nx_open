#ifndef CUSTOM_FILE_DIALOG_H
#define CUSTOM_FILE_DIALOG_H

#include <QtCore/QMap>

#include <QtGui/QFileDialog>
#include <QtGui/QCheckBox>

class QnCheckboxControlAbstractDelegate: public QObject
{
    Q_OBJECT

public:
    QnCheckboxControlAbstractDelegate(QObject *parent = 0): QObject(parent) {}
    ~QnCheckboxControlAbstractDelegate() {}

    QCheckBox* checkbox() const {
        return m_checkBox;
    }

    void setCheckbox(QCheckBox *value) {
        m_checkBox = value;
    }
public slots:
    virtual void at_filterSelected(const QString &value) = 0;
private:
    QCheckBox* m_checkBox;
};

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
     * @param delegate                  Delegate that will control state of the checkbox.
     */
    void addCheckbox(const QString &text, bool *value, QnCheckboxControlAbstractDelegate* delegate = NULL);


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
