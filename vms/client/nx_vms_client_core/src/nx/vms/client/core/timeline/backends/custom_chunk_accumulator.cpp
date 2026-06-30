// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "custom_chunk_accumulator.h"

#include <nx/utils/thread/mutex.h>

namespace nx::vms::client::core {
namespace timeline {

struct CustomChunkAccumulator::Private
{
    QnTimePeriodList chunks;
    mutable nx::Mutex mutex;
};

CustomChunkAccumulator::CustomChunkAccumulator():
    d(new Private{})
{
}

CustomChunkAccumulator::~CustomChunkAccumulator()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnTimePeriodList CustomChunkAccumulator::chunks() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->chunks;
}

bool CustomChunkAccumulator::empty() const
{
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->chunks.empty();
}

void CustomChunkAccumulator::update(const QnTimePeriod& interval, const QnTimePeriodList& chunks)
{
    {
        NX_MUTEX_LOCKER lk(&d->mutex);
        d->chunks.excludeTimePeriod(interval);
        QnTimePeriodList::unionTimePeriods(d->chunks, chunks.simplified());
    }
    emit updated();
}

void CustomChunkAccumulator::clear()
{
    {
        NX_MUTEX_LOCKER lk(&d->mutex);
        d->chunks = {};
    }
    emit updated();
}

} // namespace timeline
} // namespace nx::vms::client::core
