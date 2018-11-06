#pragma once

#include <QtWidgets/QWidget>

#include <api/model/rebuild_archive_reply.h>

#include <core/resource/resource_fwd.h>

#include <nx_ec/data/api_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QLabel;

namespace Ui {
    class ServerSettingsWidget;
}

class QnServerSettingsWidget: public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnServerSettingsWidget(QWidget* parent = nullptr);
    virtual ~QnServerSettingsWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual void retranslateUi() override;

    virtual bool canApplyChanges() const override;

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr &server);

protected:
    void setReadOnlyInternal(bool readOnly) override;

private:
    void updateFailoverLabel();
    void updateUrl();
    void updateReadOnly();

private slots:
    void at_pingButton_clicked();
private:
    QScopedPointer<Ui::ServerSettingsWidget> ui;

    QnMediaServerResourcePtr m_server;

    QPointer<QLabel> m_tableBottomLabel;
    QAction *m_removeAction;

    bool m_maxCamerasAdjusted;

    bool m_rebuildWasCanceled;
    QString m_initServerName;
};
