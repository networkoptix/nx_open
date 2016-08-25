#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui_server_updates_widget.h>

#include <utils/common/id.h>
#include <utils/common/software_version.h>

#include <update/updates_common.h>

class QnServerUpdatesModel;
class QnMediaServerUpdateTool;
struct QnLowFreeSpaceWarning;

class QnServerUpdatesWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    QnServerUpdatesWidget(QWidget* parent = nullptr);

    bool cancelUpdate();
    bool canCancelUpdate() const;
    bool isUpdating() const;

    QnMediaServerUpdateTool* updateTool() const;

    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual void loadDataToUi() override;
    virtual bool hasChanges() const override;

    virtual bool canApplyChanges() const override;
    virtual bool canDiscardChanges() const override;

private slots:
    void at_updateFinished(const QnUpdateResult& result);

    void at_tool_stageChanged(QnFullUpdateStage stage);
    void at_tool_stageProgressChanged(QnFullUpdateStage stage, int progress);
    void at_tool_lowFreeSpaceWarning(QnLowFreeSpaceWarning& lowFreeSpaceWarning);

private:
    QAction* initDropdownActions();
    void initDownloadActions();

    void autoCheckForUpdates();
    void checkForUpdates(bool fromInternet);

    QString serverNamesString(const QnMediaServerResourceList &servers);

    bool beginChecking();
    void endChecking(const QnCheckForUpdateResult& result);

private:
    QScopedPointer<Ui::QnServerUpdatesWidget> ui;

    QnServerUpdatesModel* m_updatesModel;
    QnMediaServerUpdateTool* m_updateTool;

    QnSoftwareVersion m_targetVersion;
    QnSoftwareVersion m_latestVersion;
    QString m_localFileName;
    bool m_checking;

    QUrl m_releaseNotesUrl;

    QTimer* m_longUpdateWarningTimer;

    qint64 m_lastAutoUpdateCheck;
};
