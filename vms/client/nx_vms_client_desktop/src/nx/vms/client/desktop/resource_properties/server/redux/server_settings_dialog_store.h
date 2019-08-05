#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data_fwd.h>

class QAbstractItemModel;

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
    Q_INVOKABLE bool isOnline() const;
    void setOnline(bool value);

    Q_INVOKABLE QVariantList pluginModules() const;
    void setPluginModules(const nx::vms::api::PluginInfoList& value);

    Q_INVOKABLE QVariant currentPlugin() const;
    Q_INVOKABLE QVariantList currentPluginDetails() const;
    Q_INVOKABLE void selectCurrentPlugin(const QString& libraryFilename);

    Q_INVOKABLE bool pluginsInformationLoading() const;
    void setPluginsInformationLoading(bool value);

signals:
    void stateChanged(const ServerSettingsDialogState& state);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
