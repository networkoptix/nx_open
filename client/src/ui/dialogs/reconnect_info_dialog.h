#ifndef QN_RECONNECT_INFO_DIALOG_H
#define QN_RECONNECT_INFO_DIALOG_H

#include <QtWidgets/QDialog>

#include <ui/dialogs/dialog_base.h>
#include <core/resource/resource_fwd.h>

namespace Ui {
    class ReconnectInfoDialog;
}

class QnReconnectInfoDialog: public QnDialogBase {
    Q_OBJECT

    typedef QnDialogBase base_type;
public:
    explicit QnReconnectInfoDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    virtual ~QnReconnectInfoDialog();

    bool wasCanceled() const;

    QnMediaServerResourceList servers() const;
    void setServers(const QnMediaServerResourceList &servers);

    QnMediaServerResourcePtr currentServer() const;
    void setCurrentServer(const QnMediaServerResourcePtr &server);

    virtual void reject() override;

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