#ifndef SERVER_UPDATES_WIDGET_H
#define SERVER_UPDATES_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/resource/media_server_resource.h>

namespace Ui {
class QnServerUpdatesWidget;
}

class QnServerUpdatesModel;
class QnSortedServerUpdatesModel;
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

private slots:
    void at_checkForUpdatesButton_clicked();
    void at_installSpecificBuildButton_clicked();
    void at_updateFromLocalSourceButton_clicked();
    void at_updateButton_clicked();
    void at_updateTool_peerChanged(const QnId &peerId);

    void updateUi();
    void createUpdatesDownloader();

private:
    QScopedPointer<Ui::QnServerUpdatesWidget> ui;

    QnServerUpdatesModel *m_updatesModel;
    QnSortedServerUpdatesModel *m_sortedUpdatesModel;
    QnMediaServerUpdateTool *m_updateTool;
    int m_previousToolState;
    bool m_specificBuildCheck;
};

#endif // SERVER_UPDATES_WIDGET_H
