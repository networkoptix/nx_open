#pragma once

#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <api/server_rest_connection.h>
#include <nx/cloud/cdb/api/result_code.h>
#include <nx/cloud/cdb/api/system_data.h>

namespace Ui {
class ConnectToCloudDialog;
}

class QnConnectToCloudDialogPrivate;

class QnConnectToCloudDialog : public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    explicit QnConnectToCloudDialog(QWidget* parent);
    ~QnConnectToCloudDialog();

    void accept() override;

signals:
    void bindFinished(
        nx::cdb::api::ResultCode result,
        const nx::cdb::api::SystemData &systemData,
        const rest::QnConnectionPtr &connection);

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    QScopedPointer<Ui::ConnectToCloudDialog> ui;
    QScopedPointer<QnConnectToCloudDialogPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QnConnectToCloudDialog)
    Q_DISABLE_COPY(QnConnectToCloudDialog)
};
