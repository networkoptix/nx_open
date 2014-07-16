#ifndef SERVER_UPDATES_WIDGET_H
#define SERVER_UPDATES_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <ui/widgets/settings/abstract_preferences_widget.h>

namespace Ui {
class QnServerUpdatesWidget;
}

class QnServerUpdatesModel;
class QnMediaServerUpdateTool;

class QnServerUpdatesWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    QnServerUpdatesWidget(QWidget *parent = 0);

    virtual bool confirm() override;
    virtual bool discard() override;
private slots:
    void at_checkForUpdatesButton_clicked();
    void at_installSpecificBuildButton_clicked();
    void at_updateFromLocalSourceButton_clicked();
    void at_updateButton_clicked();
    void at_updateTool_peerChanged(const QUuid &peerId);

    void updateUi();
    void createUpdatesDownloader();

private:
    bool cancelUpdate();
    bool isUpdating() const;

private:
    QScopedPointer<Ui::QnServerUpdatesWidget> ui;

    QnServerUpdatesModel *m_updatesModel;
    QnMediaServerUpdateTool *m_updateTool;
    int m_previousToolState;
    bool m_specificBuildCheck;
};

#endif // SERVER_UPDATES_WIDGET_H
