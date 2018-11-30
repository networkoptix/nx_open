#pragma once

#include <QtCore/QPointer>

#include "abstract_search_synchronizer.h"

namespace nx::vms::client::desktop {

class MotionSearchWidget;

/**
 * An utility class to synchronize Right Panel motion tab state with current media widget
 * Motion Search mode (motion chunks display on the timeline is synchronized with Motion Search).
 */
class MotionSearchSynchronizer: public AbstractSearchSynchronizer
{
public:
    MotionSearchSynchronizer(QnWorkbenchContext* context, MotionSearchWidget* motionSearchWidget,
        QObject* parent = nullptr);

private:
    void updateAreaSelection();

private:
    const QPointer<MotionSearchWidget> m_motionSearchWidget;
};

} // namespace nx::vms::client::desktop
