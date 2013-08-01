#ifndef QN_WORKBENCH_VERSION_MISMATCH_WATCHER_H
#define QN_WORKBENCH_VERSION_MISMATCH_WATCHER_H

#include <QObject>

#include <common/common_globals.h>
#include <core/resource/resource.h>
#include <utils/common/software_version.h>

#include <ui/workbench/workbench_context_aware.h>

struct QnVersionMismatchData {
    QnVersionMismatchData(): component(Qn::EnterpriseControllerComponent) {}

    Qn::SystemComponent component;
    QnSoftwareVersion version;
    QnResourcePtr resource;
};

class QnWorkbenchVersionMismatchWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT

public:
    QnWorkbenchVersionMismatchWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchVersionMismatchWatcher();

    bool hasMismatches() const;
    QList<QnVersionMismatchData> mismatchData() const;
    QnSoftwareVersion latestVersion() const;

signals:
    bool mismatchDataChanged();

private slots:
    void updateMismatchData();
    void updateHasMismatches();

private:
    QList<QnVersionMismatchData> m_mismatchData;
    bool m_hasMismatches;
};


#endif // QN_WORKBENCH_VERSION_MISMATCH_WATCHER_H
