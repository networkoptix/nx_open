#ifndef CONNECTION_TESTING_DIALOG_H
#define CONNECTION_TESTING_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtCore/QTimer>

#include <api/model/connection_info.h>
#include <core/resource/resource_fwd.h>
#include <nx_ec/ec_api.h>
#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/help/help_topics.h>


class QDataWidgetMapper;
class QStandardItemModel;

namespace Ui {
    class ConnectionTestingDialog;
}

class QnConnectionTestingDialog : public QnButtonBoxDialog {
    Q_OBJECT

public:
    explicit QnConnectionTestingDialog(QWidget *parent);
    virtual ~QnConnectionTestingDialog();

    void testConnection(const QUrl &url);

signals:
    void resourceChecked(bool success);
    void connectRequested();

private slots:
    void tick();

    void at_ecConnection_result(int reqID, ec2::ErrorCode errorCode, const QnConnectionInfo &connectionInfo);

    /**
     * Updates ui elements depending of the test result
     */
    void updateUi(bool success, const QString &details = QString(), int helpTopicId = Qn::Empty_Help);

private:
    Q_DISABLE_COPY(QnConnectionTestingDialog)

    QScopedPointer<Ui::ConnectionTestingDialog> ui;
    QTimer* m_timeoutTimer;
    QUrl m_url;
    QPushButton* m_connectButton;
};

#endif // CONNECTION_TESTING_DIALOG_H
