/**********************************************************
* 30 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef COMPATIBILITY_VERSION_INSTALLATION_DIALOG_H
#define COMPATIBILITY_VERSION_INSTALLATION_DIALOG_H

#include <QtWidgets/QDialog>

#include <utils/common/software_version.h>
#include <utils/common/connective.h>
#include <update/updates_common.h>

class QnCompatibilityVersionInstallationTool;
class QnMediaServerUpdateTool;

namespace Ui {
    class QnCompatibilityVersionInstallationDialog;
}

class QnCompatibilityVersionInstallationDialog : public Connective<QDialog> {
    Q_OBJECT

    typedef Connective<QDialog> base_type;
public:
    QnCompatibilityVersionInstallationDialog(const QnSoftwareVersion &version, QWidget *parent = 0);
    virtual ~QnCompatibilityVersionInstallationDialog();

    bool installationSucceeded() const;

    virtual int exec() override;

public slots:
    virtual void reject() override;

private slots:
    void at_compatibilityTool_statusChanged(int status);
    void at_updateTool_updateFinished(QnUpdateResult result);

private:
    int installCompatibilityVersion();
    int installUpdate();

private:
    QScopedPointer<Ui::QnCompatibilityVersionInstallationDialog> m_ui;

    QnSoftwareVersion m_versionToInstall;
    QScopedPointer<QnCompatibilityVersionInstallationTool> m_compatibilityTool;
    QScopedPointer<QnMediaServerUpdateTool> m_updateTool;

    bool m_installationOk;
};

#endif  //COMPATIBILITY_VERSION_INSTALLATION_DIALOG_H
