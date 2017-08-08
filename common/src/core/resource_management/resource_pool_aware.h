#pragma once

#include <QtCore/QObject>

#include <core/resource/resource.h>
#include <common/common_module_aware.h>

namespace nx {
namespace core {
namespace resource {

class ResourcePoolAware: public QObject, public QnCommonModuleAware
{
    Q_OBJECT;
public:
    ResourcePoolAware(QnCommonModule* commonModule);
    virtual ~ResourcePoolAware();
    virtual void resourceAdded(const QnResourcePtr& resource) = 0;
    virtual void resourceRemoved(const QnResourcePtr& resource) = 0;
};

} // namespace resource
} // namespace core
} // namespace nx

