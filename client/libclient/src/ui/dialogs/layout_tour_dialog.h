#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

#include <nx_ec/data/api_fwd.h>

namespace Ui {
class LayoutTourDialog;
}

class QnLayoutTourDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    QnLayoutTourDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = 0);
    virtual ~QnLayoutTourDialog();

    void loadData(const ec2::ApiLayoutTourData& tour);
    void submitData(ec2::ApiLayoutTourData* tour) const;

private:
    QScopedPointer<Ui::LayoutTourDialog> ui;
};
