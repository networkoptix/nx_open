// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QObject>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/software_version.h>

class QnWorkbenchVersionMismatchWatcher:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum class Component
    {
        server,
        client,
        any
    };

    struct Data
    {
        Data() = default;
        Data(Component component, const nx::utils::SoftwareVersion& version):
            component(component), version(version){}

        Component component = Component::any;
        nx::utils::SoftwareVersion version;
        QnMediaServerResourcePtr server;

        bool isValid() const { return component != Component::any; }
    };

    QnWorkbenchVersionMismatchWatcher(QObject* parent = nullptr);
    virtual ~QnWorkbenchVersionMismatchWatcher() override;

    bool hasMismatches() const;
    QList<Data> components() const;
    nx::utils::SoftwareVersion latestVersion(Component component = Component::any) const;

    static bool versionMismatches(const nx::utils::SoftwareVersion& left,
        const nx::utils::SoftwareVersion& right, bool concernBuild = false);

signals:
    bool mismatchDataChanged();

private:
    void updateComponents();
    void updateHasMismatches();

private:
    QList<Data> m_components;
    bool m_hasMismatches = false;
};
