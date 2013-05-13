#ifndef LAYOUT_SETTINGS_DIALOG_H
#define LAYOUT_SETTINGS_DIALOG_H

#include <QDialog>

#include <core/resource/resource_fwd.h>

namespace Ui {
    class QnLayoutSettingsDialog;
}

class QnLayoutSettingsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit QnLayoutSettingsDialog(QWidget *parent = 0);
    ~QnLayoutSettingsDialog();
    
    void readFromResource(const QnLayoutResourcePtr &layout);
    bool submitToResource(const QnLayoutResourcePtr &layout);
private slots:
    void at_viewButton_clicked();
    void at_selectButton_clicked();
    void at_clearButton_clicked();

    void updateControls();
private:
    bool hasChanges(const QnLayoutResourcePtr &layout);

private:
    QScopedPointer<Ui::QnLayoutSettingsDialog> ui;

    int m_imageId;
};

#endif // LAYOUT_SETTINGS_DIALOG_H
