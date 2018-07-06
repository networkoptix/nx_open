#include "camera_settings_panic_watcher.h"

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>

#include "../redux/camera_settings_dialog_store.h"

namespace nx {
namespace client {
namespace desktop {

CameraSettingsPanicWatcher::CameraSettingsPanicWatcher(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy)
{
}

void CameraSettingsPanicWatcher::setStore(CameraSettingsDialogStore* store)
{
    connect(
        context()->instance<QnWorkbenchPanicWatcher>(),
        &QnWorkbenchPanicWatcher::panicModeChanged,
        store,
        [store](bool value) { store->setPanicMode(value); });
}

} // namespace desktop
} // namespace client
} // namespace nx
