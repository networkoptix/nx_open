#ifndef TWO_STEP_FILE_DIALOG_H
#define TWO_STEP_FILE_DIALOG_H

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFileDialog>

#include <ui/dialogs/common/button_box_dialog.h>

namespace Ui {
    class QnTwoStepFileDialog;
}

class QnTwoStepFileDialog : public QnButtonBoxDialog
{
    Q_OBJECT
    typedef QnButtonBoxDialog base_type;

public:
    explicit QnTwoStepFileDialog(QWidget *parent, const QString &caption = QString(),
                                 const QString &initialFile = QString(), const QString &filter = QString());
    ~QnTwoStepFileDialog();

    void setOptions(QFileDialog::Options options);
    void setFileMode(QFileDialog::FileMode mode);
    void setAcceptMode(QFileDialog::AcceptMode mode);

    QString selectedFile() const;
    QString selectedNameFilter() const;

    static QFileDialog::Options fileDialogOptions() { return 0; }
    static QFileDialog::Options directoryDialogOptions() { return QFileDialog::ShowDirsOnly; }

    virtual int exec() override;

signals:
    void filterSelected(const QString &filter);

protected:
    virtual bool event(QEvent *event) override;

    QGridLayout *customizedLayout() const;
    void setNameFilters(const QStringList &filters);
    void updateMode();
    void updateCustomizedLayout();

    Q_SLOT void updateFileExistsWarning();
    Q_SLOT void at_browseFolderButton_clicked();
    Q_SLOT void at_browseFileButton_clicked();

private:
    QScopedPointer<Ui::QnTwoStepFileDialog> ui;

    QFileDialog::FileMode m_mode;
    QString m_filter;
    QString m_selectedExistingFilter;
};

#endif // TWO_STEP_FILE_DIALOG_H
