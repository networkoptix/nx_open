#pragma once

#include <QtGui/QStandardItemModel>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>
#include <common/common_module_aware.h>

namespace nx {

namespace vms {
namespace common {
class MetadataPluginInstanceResource;
} // namespace common
} // namespace vms
typedef QnSharedResourcePointerList<vms::common::MetadataPluginInstanceResource> MetadataPluginInstanceResourceList;

namespace client {
namespace desktop {
namespace ui {

class PluginEventModel: public QnCommonModuleAware, public QStandardItemModel
{
public:
    enum DataRole
    {
        PluginIdRole = Qt::UserRole + 1
    };

    PluginEventModel(QObject* parent = nullptr);
    ~PluginEventModel();

    void buildFromList(const MetadataPluginInstanceResourceList& pirs);

    bool isValid() const;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
