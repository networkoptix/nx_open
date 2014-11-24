#ifndef SERVER_UPDATES_WIDGET_H
#define SERVER_UPDATES_WIDGET_H

#include <QtWidgets/QWidget>

#include <array>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui_server_updates_widget.h>

#include <utils/common/id.h>
#include <utils/common/software_version.h>

#include <update/updates_common.h>

class QnServerUpdatesModel;
class QnMediaServerUpdateTool;

class QnServerUpdatesWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    QnServerUpdatesWidget(QWidget *parent = 0);

    bool cancelUpdate();
    bool isUpdating() const;

    QnMediaServerUpdateTool *updateTool() const;

    virtual void updateFromSettings() override;
    virtual bool confirm() override;
    virtual bool discard() override;

private slots:
    void at_updateFinished(const QnUpdateResult &result);

    void at_tool_stageChanged(QnFullUpdateStage stage);
    void at_tool_stageProgressChanged(QnFullUpdateStage stage, int progress);

private:
    void initSourceMenu();
    void initLinkButtons();
    void initBuildSelectionButtons();

    void autoCheckForUpdatesInternet();
    void checkForUpdatesInternet(bool autoSwitch = false, bool autoStart = false);
    void checkForUpdatesLocal();

private:
    enum UpdateSource {
        InternetSource,
        LocalSource,

        UpdateSourceCount
    };

    QScopedPointer<Ui::QnServerUpdatesWidget> ui;

    QnServerUpdatesModel *m_updatesModel;
    QnMediaServerUpdateTool *m_updateTool;
    std::array<QAction*, UpdateSourceCount> m_updateSourceActions;
  
    QnSoftwareVersion m_targetVersion;
    QnSoftwareVersion m_latestVersion;
    bool m_checkingInternet;
    bool m_checkingLocal;

    QUrl m_releaseNotesUrl;

    QTimer *m_longUpdateWarningTimer;

    qint64 m_lastAutoUpdateCheck;
};

#endif // SERVER_UPDATES_WIDGET_H
