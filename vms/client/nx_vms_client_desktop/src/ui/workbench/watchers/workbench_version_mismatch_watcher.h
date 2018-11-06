#pragma once

#include <QObject>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/software_version.h>

struct QnAppInfoMismatchData
{
    QnAppInfoMismatchData() = default;
    QnAppInfoMismatchData(Qn::SystemComponent component, const nx::utils::SoftwareVersion& version):
        component(component), version(version){}

    Qn::SystemComponent component = Qn::AnyComponent;
    nx::utils::SoftwareVersion version;
    QnResourcePtr resource;

    bool isValid() const { return component != Qn::AnyComponent; }
};

class QnWorkbenchVersionMismatchWatcher:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    QnWorkbenchVersionMismatchWatcher(QObject* parent = nullptr);
    virtual ~QnWorkbenchVersionMismatchWatcher();

    bool hasMismatches() const;
    QList<QnAppInfoMismatchData> mismatchData() const;
    nx::utils::SoftwareVersion latestVersion(Qn::SystemComponent component = Qn::AnyComponent) const;

    static bool versionMismatches(const nx::utils::SoftwareVersion& left,
        const nx::utils::SoftwareVersion& right, bool concernBuild = false);

signals:
    bool mismatchDataChanged();

private slots:
    void updateMismatchData();
    void updateHasMismatches();

private:
    QList<QnAppInfoMismatchData> m_mismatchData;
    bool m_hasMismatches;
};
