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
    void submitToResource(const QnLayoutResourcePtr &layout);

    bool hasChanges() const {return true;}

private:
    QScopedPointer<Ui::QnLayoutSettingsDialog> ui;
};

#endif // LAYOUT_SETTINGS_DIALOG_H
