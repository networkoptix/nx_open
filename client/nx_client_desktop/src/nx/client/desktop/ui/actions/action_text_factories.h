#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/device_dependent_strings.h>

#include <ui/actions/action_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class TextFactory: public QObject, public QnWorkbenchContextAware
{
public:
    /**
     * Constructor.
     *
     * \param parent                    Context-aware parent.
     */
    TextFactory(QObject *parent);

    /**
     * \param parameters                Action parameters.
     * \returns                         Actual text for the given action.
     */
    virtual QString text(const QnActionParameters& parameters) const;
};

class DevicesNameTextFactory: public TextFactory
{
public:
    DevicesNameTextFactory(const QnCameraDeviceStringSet& stringSet,
        QObject *parent = nullptr);
    virtual QString text(const QnActionParameters& parameters) const override;
private:
    const QnCameraDeviceStringSet m_stringSet;
};

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
