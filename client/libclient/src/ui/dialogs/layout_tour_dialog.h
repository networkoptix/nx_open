#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
class LayoutTourDialog;
}

class QnLayoutTourDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    virtual ~QnLayoutTourDialog();
    QnLayoutTourDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = 0);

private:
    QScopedPointer<Ui::LayoutTourDialog> ui;
};
