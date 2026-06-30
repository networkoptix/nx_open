// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <recording/time_period_list.h>

namespace nx::vms::client::core {
namespace timeline {

/**
 * Accumulates known custom chunks. Unknown intervals stay empty. When chunks for an interval are
 * received, previously known chunks for that interval are cleared & replaced with the new chunks.
 *
 * This class is used to build virtual chunks from loaded bookmarks.
 * It is thread-safe.
 */
class NX_VMS_CLIENT_CORE_API CustomChunkAccumulator: public QObject
{
    Q_OBJECT

public:
    CustomChunkAccumulator();
    virtual ~CustomChunkAccumulator() override;

    QnTimePeriodList chunks() const;
    bool empty() const;

    void update(const QnTimePeriod& interval, const QnTimePeriodList& chunks);
    void clear();

signals:
    void updated();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::core
