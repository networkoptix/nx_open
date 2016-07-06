#ifndef ATTACH_TO_VIDEOWALL_DIALOG_H
#define ATTACH_TO_VIDEOWALL_DIALOG_H

#include <client/client_model_types.h>
#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/workbench_state_dependent_dialog.h>


namespace Ui {
class QnAttachToVideowallDialog;
}

class QnAttachToVideowallDialog : public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    explicit QnAttachToVideowallDialog(QWidget *parent = 0);
    ~QnAttachToVideowallDialog();

    void loadFromResource(const QnVideoWallResourcePtr &videowall);
    void submitToResource(const QnVideoWallResourcePtr &videowall); 
private:
    void updateLicencesUsage();
private:
    QScopedPointer<Ui::QnAttachToVideowallDialog> ui;
    QnVideoWallResourcePtr m_videowall;
    bool m_valid;
};

#endif // ATTACH_TO_VIDEOWALL_DIALOG_H
