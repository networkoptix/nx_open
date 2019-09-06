#pragma once

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics {

class PropertyDescriptorStorageHelper: public Connective<QObject>
{
    Q_OBJECT
public:
    PropertyDescriptorStorageHelper(
        QnResourcePtr resource,
        QString propertyName,
        std::function<void()> propertyChangedHandler);

private:
    QnResourcePtr m_resource;
    QString m_propertyName;
    std::function<void()> m_propertyChangedHandler;
};

} // namespace nx::analytics
