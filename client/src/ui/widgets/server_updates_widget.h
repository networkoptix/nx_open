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

class QnServerUpdatesModel;
class QnMediaServerUpdateTool;

class QnServerUpdatesWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    QnServerUpdatesWidget(QWidget *parent = 0);

    bool cancelUpdate();
    bool isUpdating() const;

    void setTargets(const QSet<QUuid> &targets);

    QnMediaServerUpdateTool *updateTool() const;

    virtual void updateFromSettings() override;
    virtual bool confirm() override;
    virtual bool discard() override;

    void checkForUpdates();

private slots:
    void updateUi();

private:
    void initMenu();

private:
    enum UpdateSource {
        InternetSource,
        LocalSource,
        SpecificBuildSource,

        UpdateSourceCount
    };

    QScopedPointer<Ui::QnServerUpdatesWidget> ui;

    QnServerUpdatesModel *m_updatesModel;
    QnMediaServerUpdateTool *m_updateTool;
    int m_previousToolState;

    QTimer *m_extraMessageTimer;

    
    struct {
        UpdateSource source;
        QString filename;
        QnSoftwareVersion build;
    } m_updateInfo;

    std::array<QAction*, UpdateSourceCount> m_updateSourceActions;
};

#endif // SERVER_UPDATES_WIDGET_H
