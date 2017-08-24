#pragma once

#include <core/resource/device_dependent_strings.h>

#include <nx/client/desktop/ui/actions/action_fwd.h>

class QnWorkbenchContext;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

class TextFactory: public QObject
{
public:
    /**
     * Constructor.
     *
     * \param parent                    Context-aware parent.
     */
    TextFactory(QObject* parent);

    /**
     * \param parameters                Action parameters.
     * \returns                         Actual text for the given action.
     */
    virtual QString text(const Parameters& parameters, QnWorkbenchContext* context) const;
};

class DevicesNameTextFactory: public TextFactory
{
public:
    DevicesNameTextFactory(const QnCameraDeviceStringSet& stringSet, QObject* parent);
    virtual QString text(const Parameters& parameters, QnWorkbenchContext* context) const override;
private:
    const QnCameraDeviceStringSet m_stringSet;
};

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
