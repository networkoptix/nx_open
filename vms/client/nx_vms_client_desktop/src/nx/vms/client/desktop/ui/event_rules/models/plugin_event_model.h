#pragma once

#include <QtGui/QStandardItemModel>

#include <core/resource/resource_fwd.h>

namespace nx::vms::client::desktop {
namespace ui {

class PluginEventModel: public QStandardItemModel
{
    Q_OBJECT

public:
    enum DataRole
    {
        PluginIdRole = Qt::UserRole + 1
    };

    PluginEventModel(QObject* parent = nullptr);
    virtual ~PluginEventModel() override;

    void buildFromList(const nx::vms::common::AnalyticsEngineResourceList& engines);

    bool isValid() const;
};

} // namespace ui
} // namespace nx::vms::client::desktop
