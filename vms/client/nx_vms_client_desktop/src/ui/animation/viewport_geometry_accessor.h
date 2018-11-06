#pragma once

#include <nx/vms/client/desktop/common/utils/accessor.h>

class ViewportGeometryAccessor: public nx::vms::client::desktop::AbstractAccessor
{
public:
    virtual QVariant get(const QObject* object) const override;
    virtual void set(QObject* object, const QVariant& value) const override;
};
