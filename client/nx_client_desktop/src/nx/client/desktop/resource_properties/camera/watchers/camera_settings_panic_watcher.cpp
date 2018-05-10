#include "camera_settings_panic_watcher.h"

#include <QtCore/QPointer>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>

#include "../redux/camera_settings_dialog_store.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace client {
namespace desktop {

CameraSettingsPanicWatcher::CameraSettingsPanicWatcher(
    CameraSettingsDialogStore* store,
    QObject* parent)
    :
    base_type(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy)
{
    const auto watcher = context()->instance<QnWorkbenchPanicWatcher>();
    NX_ASSERT(watcher && store);

    store->setPanicMode(watcher->isPanicMode());

    connect(watcher, &QnWorkbenchPanicWatcher::panicModeChanged, this,
        [store = QPointer<CameraSettingsDialogStore>(store)](bool value)
        {
            if (store)
                store->setPanicMode(value);
        });
}

} // namespace desktop
} // namespace client
} // namespace nx
