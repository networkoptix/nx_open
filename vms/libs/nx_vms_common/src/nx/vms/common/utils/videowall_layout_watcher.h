// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <utils/common/counter_hash.h>

class QnResourcePool;

namespace nx::vms::common {

/**
 * This helper class watches videowalls in a resource pool and sends notification signals
 * when a videowall's set of layouts is changed. Only layout ids are watched,
 * but not actual layout presence in the resource pool.
 *
 * The class is designed the way that there's no more than one shared instance of the helper
 * per resource pool.
 */
class NX_VMS_COMMON_API VideowallLayoutWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

protected:
    explicit VideowallLayoutWatcher(QnResourcePool* resourcePool);

public:
    static std::shared_ptr<VideowallLayoutWatcher> instance(QnResourcePool* resourcePool);
    virtual ~VideowallLayoutWatcher() override;

    /** Returns all videowall layout ids. The videowall must be in the watched resource pool. */
    QnCounterHash<QnUuid> videowallLayouts(const QnVideoWallResourcePtr& videowall) const;

    /**
     * Returns layout's videowall if:
     *   - layout's parent resource is a videowall;
     *   - that videowall is in the watched resource pool;
     *   - that videowall has the layout among its items.
     */
    QnVideoWallResourcePtr layoutVideowall(const QnLayoutResourcePtr& layout) const;

    /** Returns whether the specified layoutId is present in one or more videowall items. */
    bool isVideowallItem(const QnUuid& layoutId) const;

signals:
    // These signals are sent after `videowallLayouts` are properly updated after a videowall is
    // added to or removed from the resource pool.
    void videowallAdded(const QnVideoWallResourcePtr& videowall);
    void videowallRemoved(const QnVideoWallResourcePtr& videowall);

    // These signals are sent when a set of videowall layouts is changed.
    // Only layout ids are watched, but not actual layout presence in the resource pool.
    // These signals are NOT sent when a videowall is added to or removed from the resource pool.
    void videowallLayoutsAdded(
        const QnVideoWallResourcePtr& videowall, const QVector<QnUuid>& layoutIds);
    void videowallLayoutsRemoved(
        const QnVideoWallResourcePtr& videowall, const QVector<QnUuid>& layoutIds);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::common
