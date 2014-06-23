#ifndef SERVER_UPDATES_WIDGET_H
#define SERVER_UPDATES_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/resource/media_server_resource.h>

namespace Ui {
class QnServerUpdatesWidget;
}

class QnServerUpdatesModel;
class QnMediaServerUpdateTool;

class QnServerUpdatesWidget : public QWidget {
    Q_OBJECT
public:
    QnServerUpdatesWidget(QWidget *parent = 0);
    ~QnServerUpdatesWidget();

    bool cancelUpdate();
    bool isUpdating() const;

    void setTargets(const QSet<QnId> &targets);

    QnMediaServerUpdateTool *updateTool() const;

    bool isMinimalMode() const;
    void setMinimalMode(bool minimalMode);

    void reset();

private slots:
    void at_checkForUpdatesButton_clicked();
    void at_installSpecificBuildButton_clicked();
    void at_updateFromLocalSourceButton_clicked();
    void at_updateButton_clicked();
    void at_updateTool_peerChanged(const QnId &peerId);
    void at_extraMessageTimer_timeout();

    void updateUi();

private:
    QScopedPointer<Ui::QnServerUpdatesWidget> ui;
    bool m_minimalMode;

    QnServerUpdatesModel *m_updatesModel;
    QnMediaServerUpdateTool *m_updateTool;
    int m_previousToolState;

    QTimer *m_extraMessageTimer;
};

#endif // SERVER_UPDATES_WIDGET_H
