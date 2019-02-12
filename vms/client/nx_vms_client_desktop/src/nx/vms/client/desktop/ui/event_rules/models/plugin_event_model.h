#pragma once

#include <QtGui/QStandardItemModel>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>
#include <common/common_module_aware.h>

namespace nx::vms::client::desktop {
namespace ui {

class PluginEventModel: public QnCommonModuleAware, public QStandardItemModel
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
