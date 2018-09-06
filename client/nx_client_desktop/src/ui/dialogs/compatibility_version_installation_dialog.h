#pragma once

#include <QtWidgets/QDialog>

#include <utils/common/software_version.h>
#include <utils/common/connective.h>
#include <update/updates_common.h>
#include <ui/dialogs/common/dialog.h>

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
    CompatibilityVersionInstallationDialog(const QnSoftwareVersion& version,
        QWidget* parent = nullptr);
    virtual ~CompatibilityVersionInstallationDialog();

    bool installationSucceeded() const;

    virtual int exec() override;
    virtual void reject() override;

private:
    void at_updateTool_updateFinished(QnUpdateResult result);

private:
    int installUpdate();
    void setMessage(const QString& message);

private:
    QScopedPointer<Ui::QnCompatibilityVersionInstallationDialog> m_ui;

    QnSoftwareVersion m_versionToInstall;
    QScopedPointer<QnMediaServerUpdateTool> m_updateTool;

    bool m_installationOk = false;
};
