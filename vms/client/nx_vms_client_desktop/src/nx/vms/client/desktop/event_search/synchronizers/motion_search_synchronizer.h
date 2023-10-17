// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>

#include "abstract_search_synchronizer.h"

namespace nx::vms::client::desktop {

class CommonObjectSearchSetup;
class SimpleMotionSearchListModel;

/**
 * An utility class to synchronize Right Panel motion tab state with current media widget
 * Motion Search mode (motion chunks display on the timeline is synchronized with Motion Search).
 */
class MotionSearchSynchronizer: public AbstractSearchSynchronizer
{
public:
    MotionSearchSynchronizer(
        WindowContext* context,
        CommonObjectSearchSetup* commonSetup,
        SimpleMotionSearchListModel* model,
        QObject* parent = nullptr);

private:
    void updateAreaSelection();
    void updateCachedDevices();
    virtual bool isMediaAccepted(QnMediaResourceWidget* widget) const override;

private:
    const QPointer<CommonObjectSearchSetup> m_commonSetup;
    const QPointer<SimpleMotionSearchListModel> m_model;
    bool m_layoutChanging = false;
};

} // namespace nx::vms::client::desktop
