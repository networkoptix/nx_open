// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource/layout_resource.h>

namespace nx::vms::client::desktop {

struct CrossSystemLayoutData;

class CrossSystemLayoutResource: public LayoutResource
{
    Q_OBJECT
    using base_type = LayoutResource;

public:
    CrossSystemLayoutResource();

    void update(const CrossSystemLayoutData& layoutData);

    virtual QString getProperty(const QString& key) const override;

    virtual bool setProperty(
        const QString& key,
        const QString& value,
        bool markDirty = true) override;

protected:
    /** Virtual constructor for cloning. */
    virtual LayoutResourcePtr create() const override;

    virtual bool doSaveAsync(SaveLayoutResultFunction callback) override;

private:
    QString m_customGroupId;
};

using CrossSystemLayoutResourcePtr = QnSharedResourcePointer<CrossSystemLayoutResource>;
using CrossSystemLayoutResourceList = QnSharedResourcePointerList<CrossSystemLayoutResource>;

} // namespace nx::vms::client::desktop
