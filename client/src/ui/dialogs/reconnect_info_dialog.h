#ifndef QN_RECONNECT_INFO_DIALOG_H
#define QN_RECONNECT_INFO_DIALOG_H

#include <QtWidgets/QDialog>

#include <core/resource/resource_fwd.h>

namespace Ui {
    class ReconnectInfoDialog;
}

class QnReconnectInfoDialog: public QDialog {
    Q_OBJECT

    typedef QDialog base_type;
public:
    explicit QnReconnectInfoDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnReconnectInfoDialog();

    bool wasCanceled() const;

    QnMediaServerResourcePtr currentServer() const;
    void setCurrentServer(const QnMediaServerResourcePtr &server);

signals:
    void currentServerChanged(const QnMediaServerResourcePtr &server);
private:
    Q_DISABLE_COPY(QnReconnectInfoDialog);

    QScopedPointer<Ui::ReconnectInfoDialog> ui;
    QnMediaServerResourceList m_servers;
    QnMediaServerResourcePtr m_currentServer;
    bool m_cancelled;
};

#endif //QN_RECONNECT_INFO_DIALOG_H