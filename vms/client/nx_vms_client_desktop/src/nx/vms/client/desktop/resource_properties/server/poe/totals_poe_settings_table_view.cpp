#include "totals_poe_settings_table_view.h"

#include <QtWidgets/QHeaderView>

#include "poe_settings_table_view.h"

namespace nx::vms::client::desktop {

TotalsPoeSettingsTableView::TotalsPoeSettingsTableView(QWidget* parent):
    base_type(PoeSettingsColumn::count, parent)
{
    PoeSettingsTableView::setupPoeHeader(this);
    horizontalHeader()->setFixedHeight(1);

    setHeaderDataProvider(
        [](int /*section*/, Qt::Orientation /*orientation*/, int /*role*/, QVariant& /*data*/)
        {
            return true;
        });
}

TotalsPoeSettingsTableView::~TotalsPoeSettingsTableView()
{
}

} // namespace nx::vms::client::desktop
