#ifndef QN_WORKBENCH_VERSION_MISMATCH_WATCHER_H
#define QN_WORKBENCH_VERSION_MISMATCH_WATCHER_H

#include <QObject>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>

#include <utils/common/software_version.h>
#include <utils/common/connective.h>

#include <ui/workbench/workbench_context_aware.h>

struct QnVersionMismatchData {
    QnVersionMismatchData(): component(Qn::AnyComponent) {}
    QnVersionMismatchData(Qn::SystemComponent component, QnSoftwareVersion version):
        component(component), version(version){}

    Qn::SystemComponent component;
    QnSoftwareVersion version;
    QnResourcePtr resource;

    bool isValid() const {return component != Qn::AnyComponent; }
};

class QnWorkbenchVersionMismatchWatcher: public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnWorkbenchVersionMismatchWatcher(QObject *parent = NULL);
    virtual ~QnWorkbenchVersionMismatchWatcher();

    bool hasMismatches() const;
    QList<QnVersionMismatchData> mismatchData() const;
    QnSoftwareVersion latestVersion(Qn::SystemComponent component = Qn::AnyComponent) const;

    static bool versionMismatches(QnSoftwareVersion left, QnSoftwareVersion right, bool concernBuild = false);
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
