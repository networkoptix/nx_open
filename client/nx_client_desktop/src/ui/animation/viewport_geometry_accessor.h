#pragma once

#include <nx/client/desktop/common/utils/accessor.h>

class ViewportGeometryAccessor: public nx::client::desktop::AbstractAccessor
{
public:
    virtual QVariant get(const QObject* object) const override;
    virtual void set(QObject* object, const QVariant& value) const override;
};
