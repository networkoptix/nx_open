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

    QnVideowallAttachSettings settings() const;
    void loadSettings(const QnVideowallAttachSettings &settings);

    void loadLayoutsList(const QnLayoutResourceList &layouts);

private:
    QScopedPointer<Ui::QnAttachToVideowallDialog> ui;
};

#endif // ATTACH_TO_VIDEOWALL_DIALOG_H
