#ifndef SERVER_UPDATES_WIDGET_H
#define SERVER_UPDATES_WIDGET_H

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/widgets/settings/abstract_preferences_widget.h>
#include <ui_server_updates_widget.h>

class QnServerUpdatesModel;
class QnMediaServerUpdateTool;
class QnId;

class QnServerUpdatesWidget : public QnAbstractPreferencesWidget, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    QnServerUpdatesWidget(QWidget *parent = 0);

    bool cancelUpdate();
    bool isUpdating() const;

    void setTargets(const QSet<QnId> &targets);

    QnMediaServerUpdateTool *updateTool() const;

    bool isMinimalMode() const;
    void setMinimalMode(bool minimalMode);

    virtual void updateFromSettings();
    virtual bool confirm() override;
    virtual bool discard() override;

private slots:
    void at_checkForUpdatesButton_clicked();
    void at_installSpecificBuildButton_clicked();
    void at_updateFromLocalSourceButton_clicked();
    void at_updateButton_clicked();
    void at_updateTool_peerChanged(const QnId &peerId);
    void at_extraMessageTimer_timeout();

    void updateUi();

private:
    bool cancelUpdate();
    bool isUpdating() const;

private:
    QScopedPointer<Ui::QnServerUpdatesWidget> ui;
    bool m_minimalMode;

    QnServerUpdatesModel *m_updatesModel;
    QnMediaServerUpdateTool *m_updateTool;
    int m_previousToolState;

    QTimer *m_extraMessageTimer;
};

#endif // SERVER_UPDATES_WIDGET_H
