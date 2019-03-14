#pragma once

#include <map>
#include <vector>

#include <QtWidgets/QFileDialog>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QGridLayout>

class QnSystemBasedCustomDialog: public QFileDialog
{
public:
    explicit QnSystemBasedCustomDialog(
        QWidget* parent = nullptr,
        const QString& caption = QString(),
        const QString& directory = QString(),
        const QString& filter = QString()):
        QFileDialog(parent, caption, directory, filter)
    {
    }

    QString selectedFile() const
    {
        const auto files = selectedFiles();
        if (files.empty())
            return QString();
        return files.first();
    }

    static Options fileDialogOptions()
    {
        return DontUseNativeDialog;
    }

    static Options directoryDialogOptions()
    {
        return DontUseNativeDialog | ShowDirsOnly;
    }

protected:
    QGridLayout* customizedLayout() const
    {
        return dynamic_cast<QGridLayout*>(layout());
    }
};

class QnAbstractWidgetControlDelegate: public QObject
{
    Q_OBJECT

public:
    explicit QnAbstractWidgetControlDelegate(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    QList<QWidget*> widgets() const
    {
        return m_widgets;
    }

    void addWidget(QWidget* value)
    {
        m_widgets << value;
    }

    virtual void updateWidget(const QString& value) = 0;

private:
    QList<QWidget*> m_widgets;
};

class QnCustomFileDialog: public QnSystemBasedCustomDialog
{
    Q_OBJECT
    using base_type = QnSystemBasedCustomDialog;

public:
    explicit QnCustomFileDialog(
        QWidget* parent = nullptr,
        const QString& caption = QString(),
        const QString& directory = QString(),
        const QString& filter = QString());
    ~QnCustomFileDialog();

    using FileFilter = std::pair<QString, QStringList>;

    static FileFilter picturesFilter();
    static const FileFilter kVideoFilter;
    static const FileFilter kAllFilesFilter;

    /**
     * Creates a filter string, which will be expanded to the list of allowed file formats.
     * @param filters Each value represents a separate filter line, where first element is a
     *      textual description, and the second is the list of extensions (without dot).
     * @return Filter string in the QFileDialog format.
     */
    static QString createFilter(std::vector<FileFilter> filters);

 /**
     * Creates a filter string, which will be expanded to the list of allowed file formats.
     * @param filter A pair of textual description and the list of extensions (without dot).
     * @return Filter string in the QFileDialog format.
     */
    static QString createFilter(FileFilter filter);

    /**
     * Creates a filter string, which will be expanded to the list of allowed file formats.
     * @param title Textual description for the file formats.
     * @param extensions List of extensions (without dot).
     * @return Filter string in the QFileDialog format.
     */
    static QString createFilter(QString title, QStringList extensions);

    /**
     * Creates a filter string, which will be expanded to the list of allowed file formats.
     * @param title Textual description for the file formats.
     * @param extension Single extension (without dot).
     * @return Filter string in the QFileDialog format.
     */
    static QString createFilter(QString title, const QString& extension);

    /**
     * Adds a checkbox to this file dialog.
     * @param text Checkbox text.
     * @param value Pointer to the initial value of the checkbox. This pointer will be saved and
     *      when the dialog closes, resulting checkbox value will be written into it. It is the
     *      callee's responsibility to make sure that pointed-to value still exists at that point.
     * @param delegate Delegate that will control state of the checkbox. Dialog does not take the
     *      ownership of the delegate, it must be freed by the callee.
     */
    void addCheckBox(
        const QString& text,
        bool* value,
        QnAbstractWidgetControlDelegate* delegate = nullptr);

    /**
     * Adds a spinbox to this file dialog.
     * @param text Text that will be displayed on a label before the spinbox.
     *      If ::spinBoxPlaceholder() is used within the text, spinbox will be placed in the middle
     *      of the text (in the position of the placeholder).
     * @param minValue Minimum value
     * @param maxValue Maximum value
     * @param value Pointer to the initial value. This pointer will be saved and when the dialog
     *      closes, resulting spinbox value will be written into it. It is the callee's
     *      responsibility to make sure that pointed-to value still exists at that point.
     */
    void addSpinBox(const QString& text, int minValue, int maxValue, int* value);

    /**
     * Placeholder where spinbox should be added when creating an inline-spinbox.
     */
    static QString spinBoxPlaceholder();

    /**
     * Adds a lineedit to this file dialog.
     * @param label Text that will be displayed on a label before the lineedit.
     * @param value Pointer to the initial value. This pointer will be saved and when the dialog
     *      closes, resulting lineedit value will be written into it. It is the callee's
     *      responsibility to make sure that pointed-to value still exists at that point.
     */
    void addLineEdit(const QString& label, QString* value);

    /**
     * Adds custom widget to this file dialog.
    * @param label Text that will be displayed on a label before the widget.
     * @param widget Pointer to the widget.
     * @param delegate Delegate that will control state of the widget. Dialog does not take the
     *      ownership of the delegate, it must be freed by the callee.
     */
    void addWidget(
        const QString& label,
        QWidget* widget,
        QnAbstractWidgetControlDelegate* delegate = nullptr);

    /**
     * @return extension of the selected filter, containing the dot, e.g. ".png".
     */
    QString selectedExtension() const;

private:
    void at_accepted();

private:
    std::map<QCheckBox*, bool*> m_checkBoxes;
    std::map<QSpinBox*, int*> m_spinBoxes;
    std::map<QLineEdit*, QString*> m_lineEdits;
    int m_currentColumn = 0;
};
