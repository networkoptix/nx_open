#ifndef TWO_STEP_FILE_DIALOG_H
#define TWO_STEP_FILE_DIALOG_H

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFileDialog>

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
class QnTwoStepFileDialog;
}

class QnTwoStepFileDialog : public QnButtonBoxDialog
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
public:
    explicit QnTwoStepFileDialog(QWidget *parent = 0, const QString &caption = QString(), const QString &initialFile = QString(), const QString &filter = QString());
    ~QnTwoStepFileDialog();

    void setOptions(QFileDialog::Options options);
    void setFileMode(QFileDialog::FileMode mode);
    void setAcceptMode(QFileDialog::AcceptMode mode);

    QString selectedFile() const;
    QString selectedNameFilter() const;

    static QFileDialog::Options fileDialogOptions() { return 0; }
    static QFileDialog::Options directoryDialogOptions() { return QFileDialog::ShowDirsOnly; }
signals:
    void filterSelected(const QString &filter);

protected:
    QGridLayout* customizedLayout() const;
    void setNameFilters(const QStringList &filters);
    Q_SLOT void updateFileExistsWarning();
    Q_SLOT void at_browseButton_clicked();

private:
    QScopedPointer<Ui::QnTwoStepFileDialog> ui;
};

#endif // TWO_STEP_FILE_DIALOG_H
