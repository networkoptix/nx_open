#ifndef MERGE_SYSTEMS_DIALOG_H
#define MERGE_SYSTEMS_DIALOG_H

#include <QtWidgets/QDialog>

namespace Ui {
class QnMergeSystemsDialog;
}

class QnMergeSystemsDialog : public QDialog {
    Q_OBJECT

public:
    explicit QnMergeSystemsDialog(QWidget *parent = 0);
    ~QnMergeSystemsDialog();

    QUrl url() const;
    QString password() const;
    void updateKnownSystems();

private slots:
    void at_urlComboBox_activated(int index);
    void at_testConnectionButton_clicked();

private:
    QScopedPointer<Ui::QnMergeSystemsDialog> ui;
};

#endif // MERGE_SYSTEMS_DIALOG_H
