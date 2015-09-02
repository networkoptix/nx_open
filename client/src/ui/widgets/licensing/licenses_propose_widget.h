#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnLicenseUsageHelper;

namespace Ui {
    class LicensesProposeWidget;
}

class QnLicensesProposeWidget: public QWidget, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnLicensesProposeWidget(QWidget *parent);
    ~QnLicensesProposeWidget();

    QnVirtualCameraResourceList cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    Qt::CheckState state() const;
    void setState(Qt::CheckState value);

signals:
    void changed();

protected:
    void afterContextInitialized() override;

private:
    void updateLicenseText();
    void updateLicensesButtonVisible();

private:
    QScopedPointer<Ui::LicensesProposeWidget> ui;
    QnVirtualCameraResourceList m_cameras;
};
