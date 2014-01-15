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
    explicit QnTwoStepFileDialog(QWidget *parent = 0, const QString &caption = QString(), const QString &directory = QString(), const QString &filter = QString());
    ~QnTwoStepFileDialog();

    void setOption(QFileDialog::Option option, bool on = true);
    void setOptions(QFileDialog::Options options);

    void setFileMode(QFileDialog::FileMode mode);
    QFileDialog::FileMode fileMode() const;

    void setAcceptMode(QFileDialog::AcceptMode mode);
    QFileDialog::AcceptMode acceptMode() const;

    QString selectedFile() const;
    QString selectedNameFilter() const;

    static QFileDialog::Options fileDialogOptions() { return 0; }
    static QFileDialog::Options directoryDialogOptions() {return QFileDialog::ShowDirsOnly; }
signals:
    void filterSelected(const QString &filter);

protected:
    QGridLayout* customizedLayout() const;


private:
    QScopedPointer<Ui::QnTwoStepFileDialog> ui;
    QScopedPointer<QFileDialog> dialog;
};

#endif // TWO_STEP_FILE_DIALOG_H
