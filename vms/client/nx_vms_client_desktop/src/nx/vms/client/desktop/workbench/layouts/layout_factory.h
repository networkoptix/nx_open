// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>
#include <nx/utils/singleton.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>

class QnWorkbench;
class QnWorkbenchLayout;

namespace nx::vms::client::desktop {
namespace ui {
namespace workbench {

class LayoutsFactory:
    public QObject,
    public Singleton<LayoutsFactory>
{
    Q_OBJECT
    using base_type = QObject;

public:
    using LayoutCreator =
        std::function<QnWorkbenchLayout* (const LayoutResourcePtr& resource, QObject* parent)>;

    static LayoutsFactory* instance(QnWorkbench* workbench);

    LayoutsFactory(QObject* parent = nullptr);

    QnWorkbenchLayout* create(
        const LayoutResourcePtr& resource,
        QObject* parent = nullptr);

    /**
     * @brief addCreator Adds creation function
     * @param creator Creation function. Should return constructed layout or nullptr
     * if specified layout resource does not fit inner criterias or constraints.
     */
    void addCreator(const LayoutCreator& creator);

private:
    using CreatorsList = QList<LayoutCreator>;
    CreatorsList m_creators;
};

} // namespace workbench
} // namespace ui
} // namespace nx::vms::client::desktop

#define qnWorkbenchLayoutsFactory \
    nx::vms::client::desktop::ui::workbench::LayoutsFactory::instance(workbench())
