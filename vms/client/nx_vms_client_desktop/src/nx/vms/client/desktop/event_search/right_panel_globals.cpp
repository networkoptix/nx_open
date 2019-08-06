#include "right_panel_globals.h"

#include <QtQml/QtQml>

namespace nx::vms::client::desktop {
namespace RightPanel {

void registerQmlType()
{
    qmlRegisterUncreatableMetaObject(staticMetaObject, "nx.vms.client.desktop", 1, 0,
        "RightPanel", "RightPanel is a namespace");

    qRegisterMetaType<Tab>();
    qRegisterMetaType<FetchDirection>();
    qRegisterMetaType<FetchResult>();
    qRegisterMetaType<PreviewState>();
}

} // namespace RightPanel
} // namespace nx::vms::client::desktop
