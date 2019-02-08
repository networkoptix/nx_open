#pragma once

#include <QtCore/QPointer>

#include "abstract_search_synchronizer.h"

namespace nx::vms::client::desktop {

class SimpleMotionSearchWidget;

/**
 * An utility class to synchronize Right Panel motion tab state with current media widget
 * Motion Search mode (motion chunks display on the timeline is synchronized with Motion Search).
 */
class MotionSearchSynchronizer: public AbstractSearchSynchronizer
{
public:
    MotionSearchSynchronizer(QnWorkbenchContext* context,
        SimpleMotionSearchWidget* motionSearchWidget, QObject* parent = nullptr);

private:
    void updateAreaSelection();
    virtual bool isMediaAccepted(QnMediaResourceWidget* widget) const override;

private:
    const QPointer<SimpleMotionSearchWidget> m_motionSearchWidget;
};

} // namespace nx::vms::client::desktop
