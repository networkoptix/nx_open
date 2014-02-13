#ifndef CONNECTION_TESTING_DIALOG_H
#define CONNECTION_TESTING_DIALOG_H

#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <QtWidgets/QDialog>

#include <api/model/connection_info.h>

class QDataWidgetMapper;
class QStandardItemModel;

namespace Ui {
    class ConnectionTestingDialog;
}

class QnConnectionTestingDialog : public QDialog {
    Q_OBJECT

public:
    explicit QnConnectionTestingDialog(QWidget *parent = NULL);
    virtual ~QnConnectionTestingDialog();

    void testEnterpriseController(const QUrl &url);

signals:
    void resourceChecked(bool success);

private:
    Q_SLOT void tick();

    Q_SLOT void at_ecConnection_result(int status, QnConnectionInfoPtr connectionInfo, int requestHandle);

    /**
     * Updates ui elements depending of the test result
     */
    void updateUi(bool success, const QString &details = QString());

private:
    Q_DISABLE_COPY(QnConnectionTestingDialog)

    QScopedPointer<Ui::ConnectionTestingDialog> ui;
    QTimer* m_timeoutTimer;
    QUrl m_url;
};

#endif // CONNECTION_TESTING_DIALOG_H
