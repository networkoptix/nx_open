/**********************************************************
* 30 sep 2013
* a.kolesnikov
***********************************************************/
#pragma once

#include <QtWidgets/QDialog>

#include <utils/common/connective.h>
#include <update/updates_common.h>
#include <ui/dialogs/common/dialog.h>

#include <nx/vms/api/data/software_version.h>

class QnCompatibilityVersionInstallationTool;
class QnMediaServerUpdateTool;

namespace Ui {
    class QnCompatibilityVersionInstallationDialog;
}

// TODO: #dklychkov rename class in 2.4
class CompatibilityVersionInstallationDialog: public Connective<QnDialog>
{
    Q_OBJECT
    using base_type = Connective<QnDialog>;

public:
    CompatibilityVersionInstallationDialog(
        const nx::vms::api::SoftwareVersion& version, QWidget* parent = nullptr);
    virtual ~CompatibilityVersionInstallationDialog();

    bool installationSucceeded() const;

    virtual int exec() override;

    static bool useUpdate(const nx::vms::api::SoftwareVersion& version);

public slots:
    virtual void reject() override;

private slots:
    void at_compatibilityTool_statusChanged(int status);
    void at_updateTool_updateFinished(QnUpdateResult result);

private:
    int installCompatibilityVersion();
    int installUpdate();
    void setMessage(const QString& message);

private:
    QScopedPointer<Ui::QnCompatibilityVersionInstallationDialog> m_ui;

    nx::vms::api::SoftwareVersion m_versionToInstall;
    QScopedPointer<QnCompatibilityVersionInstallationTool> m_compatibilityTool;
    QScopedPointer<QnMediaServerUpdateTool> m_updateTool;

    bool m_installationOk;
};
