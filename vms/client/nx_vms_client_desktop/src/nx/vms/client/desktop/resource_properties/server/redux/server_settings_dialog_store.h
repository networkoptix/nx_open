#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/vms/api/data_fwd.h>

namespace nx::vms::client::desktop {

struct ServerSettingsDialogState;

class ServerSettingsDialogStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit ServerSettingsDialogStore(QObject* parent = nullptr);
    virtual ~ServerSettingsDialogStore() override;

    const ServerSettingsDialogState& state() const;

    void loadServer(const QnMediaServerResourcePtr& server);

    // Actions.
    Q_INVOKABLE QVariantList pluginModules() const;
    void setPluginModules(const nx::vms::api::PluginModuleDataList& value);
    Q_INVOKABLE QString currentPluginLibraryName() const;
    Q_INVOKABLE void setCurrentPluginLibraryName(const QString& value);
    Q_INVOKABLE bool pluginsInformationLoading() const;
    void setPluginsInformationLoading(bool value);

signals:
    void stateChanged(const ServerSettingsDialogState& state);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
