#pragma once

#include <QtCore/QScopedPointer>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class CredentialsStoreDialog; }

namespace nx::vms::client::desktop {

class CredentialsStoreDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    explicit CredentialsStoreDialog(QWidget* parent = nullptr);
    virtual ~CredentialsStoreDialog();

private:
    struct Private;
    QScopedPointer<Private> d;
    QScopedPointer<Ui::CredentialsStoreDialog> ui;
};

} // namespace nx::vms::client::desktop
