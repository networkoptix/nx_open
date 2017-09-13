#pragma once

#include <QtCore/QUrl>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_state_manager.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/id.h>
#include <utils/common/software_version.h>

#include <update/updates_common.h>

class QnServerUpdatesModel;
class QnMediaServerUpdateTool;
struct QnLowFreeSpaceWarning;

namespace Ui {
    class QnServerUpdatesWidget;
} // namespace Ui

class QnServerUpdatesWidget: public QnAbstractPreferencesWidget, public QnSessionAwareDelegate
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    QnServerUpdatesWidget(QWidget* parent = nullptr);
    ~QnServerUpdatesWidget();

    virtual bool tryClose(bool force) override;
    virtual void forcedUpdate() override;

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

    void at_updateFinished(const QnUpdateResult& result);

    void at_tool_stageChanged(QnFullUpdateStage stage);
    void at_tool_stageProgressChanged(QnFullUpdateStage stage, int progress);
    void at_tool_lowFreeSpaceWarning(QnLowFreeSpaceWarning& lowFreeSpaceWarning);
    void at_tool_updatesCheckCanceled();

private:
    enum class Mode
    {
        LatestVersion,
        SpecificBuild,
        LocalFile
    };
    void setMode(Mode mode);

    void initDropdownActions();
    void initDownloadActions();

    void updateButtonText();
    void updateButtonAccent();
    void updateDownloadButton();
    void updateVersionPage();

    void autoCheckForUpdates();
    void checkForUpdates(bool fromInternet);
    void refresh();

    QString serverNamesString(const QnMediaServerResourceList& servers);

    bool beginChecking();
    void endChecking(const QnCheckForUpdateResult& result);

    bool restartClient(const QnSoftwareVersion& version);

private:
    QScopedPointer<Ui::QnServerUpdatesWidget> ui;

    Mode m_mode = Mode::LatestVersion;

    QnServerUpdatesModel* m_updatesModel = nullptr;
    QnMediaServerUpdateTool* m_updateTool = nullptr;

    QnSoftwareVersion m_targetVersion;
    QString m_targetChangeset;

    QnSoftwareVersion m_latestVersion;
    QString m_localFileName;
    bool m_checking = false;
    QnCheckForUpdateResult m_lastUpdateCheckResult;

    QUrl m_releaseNotesUrl;

    QTimer* m_longUpdateWarningTimer = nullptr;

    qint64 m_lastAutoUpdateCheck = 0;
};
