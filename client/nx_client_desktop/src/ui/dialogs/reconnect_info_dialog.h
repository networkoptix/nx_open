#pragma once

#include <QtWidgets/QDialog>

#include <ui/dialogs/common/button_box_dialog.h>
#include <core/resource/resource_fwd.h>

namespace Ui {
class ReconnectInfoDialog;
}

class QnReconnectInfoDialog: public QnButtonBoxDialog
{
    Q_OBJECT

    using base_type = QnButtonBoxDialog;
public:
    explicit QnReconnectInfoDialog(QWidget *parent = NULL,
        Qt::WindowFlags windowFlags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    virtual ~QnReconnectInfoDialog();

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
};
