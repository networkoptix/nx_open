// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_accessor.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/layout_resource.h>

namespace nx::vms::client::core {
namespace resource {

//-------------------------------------------------------------------------------------------------

class LayoutAccessor::Private: public QObject
{
    LayoutAccessor* q = nullptr;

public:
    Private(LayoutAccessor* parent);

public:
    QnLayoutResourcePtr layout;
};

LayoutAccessor::Private::Private(LayoutAccessor* parent):
    q(parent)
{
}

//-------------------------------------------------------------------------------------------------

LayoutAccessor::LayoutAccessor(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

LayoutAccessor::~LayoutAccessor()
{
}

QString LayoutAccessor::layoutId() const
{
    return d->layout ? d->layout->getId().toString() : QString();
}

void LayoutAccessor::setLayoutId(const QString& id)
{
    const auto layout =
        resourcePool()->getResourceById<QnLayoutResource>(nx::Uuid::fromStringSafe(id));
    setLayout(layout);
}

QnLayoutResourcePtr LayoutAccessor::layout() const
{
    return d->layout;
}

void LayoutAccessor::setLayout(const QnLayoutResourcePtr& layout)
{
    if (d->layout == layout)
        return;

    emit layoutAboutToBeChanged();

    if (d->layout)
    {
        d->layout->disconnect(this);
    }

    d->layout = layout;

    if (layout)
    {
        connect(layout.get(), &QnLayoutResource::nameChanged, this, &LayoutAccessor::nameChanged);
    }

    emit layoutChanged();
    emit nameChanged();
}

QString LayoutAccessor::name() const
{
    return d->layout ? d->layout->getName() : QString();
}

} // namespace resource
} // namespace nx::vms::client::core
