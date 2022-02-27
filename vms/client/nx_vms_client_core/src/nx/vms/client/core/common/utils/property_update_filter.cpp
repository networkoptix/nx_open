// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "property_update_filter.h"

#include <QtQml/QtQml>

#include <nx/utils/pending_operation.h>

namespace nx::vms::client::core {

struct PropertyUpdateFilter::Private
{
    QQmlProperty target;
    QVariant source;
    bool enabled = true;
    UpdateFlags updateFlags = UpdateFlag::UpdateOnlyWhenIdle;
    nx::utils::PendingOperation updateOperation;

    void updateOperationFlags()
    {
        nx::utils::PendingOperation::Flags flags{};
        if (updateFlags.testFlag(ImmediateFirstUpdate))
            flags.setFlag(nx::utils::PendingOperation::FireImmediately);
        if (updateFlags.testFlag(UpdateOnlyWhenIdle))
            flags.setFlag(nx::utils::PendingOperation::FireOnlyWhenIdle);

        updateOperation.setFlags(flags);
    }
};

PropertyUpdateFilter::PropertyUpdateFilter(QObject* parent):
    QObject(parent),
    d(new Private())
{
    d->updateOperation.setCallback([this]() { d->target.write(d->source); });
    d->updateOperation.setIntervalMs(200);
    d->updateOperationFlags();
}

PropertyUpdateFilter::~PropertyUpdateFilter()
{
    // Required here for forward-declared scoped pointer destruction.
}

void PropertyUpdateFilter::setTarget(const QQmlProperty& value)
{
    d->target = value;
}

QVariant PropertyUpdateFilter::source() const
{
    return d->source;
}

void PropertyUpdateFilter::setSource(const QVariant& value)
{
    if (d->source == value)
        return;

    d->source = value;
    d->updateOperation.requestOperation();
}

bool PropertyUpdateFilter::enabled() const
{
    return d->enabled;
}

void PropertyUpdateFilter::setEnabled(bool value)
{
    if (d->enabled == value)
        return;

    d->enabled = value;
    emit enabledChanged();

    if (d->enabled)
        d->updateOperation.requestOperation();
    else
        d->updateOperation.cancel();
}

int PropertyUpdateFilter::minimumIntervalMs() const
{
    return d->updateOperation.intervalMs();
}

void PropertyUpdateFilter::setMinimumIntervalMs(int value)
{
    if (minimumIntervalMs() == value)
        return;

    d->updateOperation.setIntervalMs(value);
    emit minimumIntervalChanged();
}

PropertyUpdateFilter::UpdateFlags PropertyUpdateFilter::updateFlags() const
{
    return d->updateFlags;
}

void PropertyUpdateFilter::setUpdateFlags(UpdateFlags value)
{
    if (d->updateFlags == value)
        return;

    d->updateFlags = value;
    d->updateOperationFlags();
    emit updateFlagsChanged();
}

void PropertyUpdateFilter::registerQmlType()
{
    qmlRegisterType<PropertyUpdateFilter>("nx.vms.client.core", 1, 0, "PropertyUpdateFilter");
}

} // namespace nx::vms::client::core
