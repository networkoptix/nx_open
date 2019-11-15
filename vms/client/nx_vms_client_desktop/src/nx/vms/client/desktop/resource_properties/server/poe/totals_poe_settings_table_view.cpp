#include "totals_poe_settings_table_view.h"

#include <QtWidgets/QHeaderView>

#include "poe_settings_table_view.h"

namespace nx::vms::client::desktop {
namespace settings {

TotalsPoESettingsTableView::TotalsPoESettingsTableView(QWidget* parent):
    base_type(PoESettingsColumn::count, parent)
{
    PoESettingsTableView::setupHeader(this);
    horizontalHeader()->setFixedHeight(1);

    setHeaderDataProvider(
        [](int /*section*/, Qt::Orientation /*orientation*/, int /*role*/, QVariant& /*data*/)
        {
            return true;
        });
}

TotalsPoESettingsTableView::~TotalsPoESettingsTableView()
{
}

} // namespace settings
} // namespace nx::vms::client::desktop
