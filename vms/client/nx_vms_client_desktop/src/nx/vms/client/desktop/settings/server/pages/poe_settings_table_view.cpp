#include "poe_settings_table_view.h"

#include <utils/math/math.h>

namespace {

using namespace nx::vms::client::desktop::settings;

QString headerText(int column)
{
    switch(column)
    {
        case PoESettingsColumn::port:
            return PoESettingsTableView::tr("Port");
        case PoESettingsColumn::camera:
            return PoESettingsTableView::tr("Camera");
        case PoESettingsColumn::consumption:
            return PoESettingsTableView::tr("Consumption");
        case PoESettingsColumn::speed:
            return PoESettingsTableView::tr("Speed");
        case PoESettingsColumn::status:
            return PoESettingsTableView::tr("Status");
        case PoESettingsColumn::power:
            return PoESettingsTableView::tr("Power");
        default:
            return QString();
    }
}

const auto kHeaderDataProvider =
    [](int column, Qt::Orientation orientation, int role, QVariant& data)
    {
        if (orientation != Qt::Horizontal || !qBetween<int>(0, column, PoESettingsColumn::count))
            return false;

        switch(role)
        {
            case Qt::DisplayRole:
                data = headerText(column);
                return true;
            default:
                return false;
        }
    };

} // namespace

namespace nx::vms::client::desktop {
namespace settings {

PoESettingsTableView::PoESettingsTableView(QWidget* parent):
    base_type(PoESettingsColumn::count, parent)
{
    setHeaderDataProvider(kHeaderDataProvider);


}

} // namespace settings
} // namespace nx::vms::client::desktop
