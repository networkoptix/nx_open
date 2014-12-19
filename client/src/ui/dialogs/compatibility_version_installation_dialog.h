/**********************************************************
* 30 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef COMPATIBILITY_VERSION_INSTALLATION_DIALOG_H
#define COMPATIBILITY_VERSION_INSTALLATION_DIALOG_H

#include <QtWidgets/QDialog>

#include <utils/common/software_version.h>
#include <utils/common/connective.h>

class QnCompatibilityVersionInstallationTool;

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

private:
    int installCompatibilityVersion();
    int installUpdate();

private:
    QScopedPointer<Ui::QnCompatibilityVersionInstallationDialog> m_ui;

    QnSoftwareVersion m_versionToInstall;
    QScopedPointer<QnCompatibilityVersionInstallationTool> m_compatibilityTool;

    bool m_installationOk;
};

#endif  //COMPATIBILITY_VERSION_INSTALLATION_DIALOG_H
