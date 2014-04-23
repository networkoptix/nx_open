#ifndef CONNECTION_TESTING_DIALOG_H
#define CONNECTION_TESTING_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtCore/QTimer>

#include <api/model/connection_info.h>
#include <core/resource/resource_fwd.h>
#include <nx_ec/ec_api.h>
#include <ui/dialogs/button_box_dialog.h>


class QDataWidgetMapper;
class QStandardItemModel;

namespace Ui {
    class ConnectionTestingDialog;
}

class QnConnectionTestingDialog : public QnButtonBoxDialog {
    Q_OBJECT

public:
    explicit QnConnectionTestingDialog(QWidget *parent = NULL);
    virtual ~QnConnectionTestingDialog();

    void testEnterpriseController(const QUrl &url);

signals:
    void resourceChecked(bool success);

private slots:
    void tick();

    void at_ecConnection_result(int reqID, ec2::ErrorCode errorCode, QnConnectionInfo connectionInfo);

    /**
     * Updates ui elements depending of the test result
     */
    void updateUi(bool success, const QString &details = QString(), int helpTopicId = -1);

private:
    Q_DISABLE_COPY(QnConnectionTestingDialog)

    QScopedPointer<Ui::ConnectionTestingDialog> ui;
    QTimer* m_timeoutTimer;
    QUrl m_url;
};

#endif // CONNECTION_TESTING_DIALOG_H
