#ifndef SERVER_UPDATES_WIDGET_H
#define SERVER_UPDATES_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/resource/media_server_resource.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class QnServerUpdatesWidget;
}

class QnServerUpdatesModel;
class QnMediaServerUpdateTool;

class QnServerUpdatesWidget : public QWidget, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnServerUpdatesWidget(QnWorkbenchContext *context, QWidget *parent = 0);
    ~QnServerUpdatesWidget();

private slots:
    void at_checkForUpdatesButton_clicked();
    void at_installSpecificBuildButton_clicked();
    void at_updateFromLocalSourceButton_clicked();
    void at_updateButton_clicked();
    void at_updateTool_serverProgressChanged(const QnMediaServerResourcePtr &server, int progress);

    void updateUpdatesList();
    void updateUi();
    void createUpdatesDownloader();

private:
    QScopedPointer<Ui::QnServerUpdatesWidget> ui;

    QnServerUpdatesModel *m_updatesModel;
    QnMediaServerUpdateTool *m_updateTool;
};

#endif // SERVER_UPDATES_WIDGET_H
