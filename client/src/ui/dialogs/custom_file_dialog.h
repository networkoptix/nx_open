#ifndef CUSTOM_FILE_DIALOG_H
#define CUSTOM_FILE_DIALOG_H

#include <QtCore/QMap>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLineEdit>

class QnWidgetControlAbstractDelegate: public QObject
{
    Q_OBJECT

public:
    QnWidgetControlAbstractDelegate(QObject *parent = 0): QObject(parent) {}
    ~QnWidgetControlAbstractDelegate() {}

    QList<QWidget*> widgets() const {
        return m_widgets;
    }

    void addWidget(QWidget *value) {
        m_widgets << value;
    }

public slots:
    virtual void at_filterSelected(const QString &value) = 0;

private:
    QList<QWidget*> m_widgets;
};

class QnCustomFileDialog : public QFileDialog
{
    Q_OBJECT

    typedef QFileDialog base_type;

public:
    explicit QnCustomFileDialog(QWidget *parent = 0, const QString &caption = QString(), const QString &directory = QString(), const QString &filter = QString());
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
    void addCheckBox(const QString &text, bool *value, QnWidgetControlAbstractDelegate* delegate = NULL);

    /**
     * @brief addSpinBox                Adds a spinbox to this file dialog.
     * @param text                      Text that will be displaed on a label before spinbox.
     *                                  If %n is used within the text, spinbox will be placed in the
     *                                  middle of the text (in the position of %n).
     * @param minValue                  Minimum value
     * @param maxValue                  Maximum value
     * @param value                     Pointer to the initial value.
     *                                  This pointer will be saved and when the dialog
     *                                  closes, resulting spinbox value will be written into it.
     *                                  It is the callee's responsibility to make sure
     *                                  that pointed-to value still exists at that point.
     */
    void addSpinBox(const QString &text, int minValue, int maxValue, int *value);

    void addLineEdit(const QString& text, QString *value);

    /**
     * @brief addWidget                 Adds custom widget to this file dialog.
     * @param widget                    Pointer to the widget.
     */
    void addWidget(QWidget *widget, bool newRow = true, QnWidgetControlAbstractDelegate* delegate = NULL);

private slots:
    void at_accepted();

private:
    QMap<QCheckBox*, bool *> m_checkBoxes;
    QMap<QSpinBox*, int *> m_spinBoxes;
    QMap<QLineEdit*, QString *> m_lineEdits;
    int m_currentCol;
};

#endif // CUSTOM_FILE_DIALOG_H
