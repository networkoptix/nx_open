#include <nx/vms/client/desktop/application.h>
#include <nx/utils/app_info.h>

int main(int argc, char** argv)
{
    if (nx::utils::AppInfo::isMacOsX())
    {
        // We do not rely on Mac OS OpenGL implementation-related throttling.
        // Otherwise all animations go faster.
        qputenv("QSG_RENDER_LOOP", "basic");
    }

    return nx::vms::client::desktop::runApplication(argc, argv);
}
