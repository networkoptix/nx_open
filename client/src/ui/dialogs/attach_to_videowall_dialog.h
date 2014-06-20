#ifndef ATTACH_TO_VIDEOWALL_DIALOG_H
#define ATTACH_TO_VIDEOWALL_DIALOG_H

#include <client/client_model_types.h>
#include <core/resource/resource_fwd.h>
#include <ui/dialogs/button_box_dialog.h>


namespace Ui {
class QnAttachToVideowallDialog;
}

class QnAttachToVideowallDialog : public QnButtonBoxDialog
{
    Q_OBJECT

public:
    explicit QnAttachToVideowallDialog(QWidget *parent = 0);
    ~QnAttachToVideowallDialog();

    void loadFromResource(const QnVideoWallResourcePtr &videowall);
    void submitToResource(const QnVideoWallResourcePtr &videowall); 
private:
    QScopedPointer<Ui::QnAttachToVideowallDialog> ui;
};

#endif // ATTACH_TO_VIDEOWALL_DIALOG_H
