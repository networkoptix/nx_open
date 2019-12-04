#include "poe_settings_table_view.h"

#include <QtWidgets/QHeaderView>

#include <nx/vms/client/desktop/node_view/resource_node_view/details/resource_node_view_item_delegate.h>
#include <utils/math/math.h>

namespace {

using namespace nx::vms::client::desktop;

QString headerText(int column)
{
    switch(column)
    {
        case PoeSettingsColumn::port:
            return PoeSettingsTableView::tr("Port");
        case PoeSettingsColumn::camera:
            return PoeSettingsTableView::tr("Camera");
        case PoeSettingsColumn::consumption:
            return PoeSettingsTableView::tr("Consumption");
        case PoeSettingsColumn::speed:
            return PoeSettingsTableView::tr("Speed");
        case PoeSettingsColumn::status:
            return PoeSettingsTableView::tr("Status");
        case PoeSettingsColumn::power:
            return PoeSettingsTableView::tr("Power");
        default:
            return QString();
    }
}

const auto kHeaderDataProvider =
    [](int column, Qt::Orientation orientation, int role, QVariant& data)
    {
        if (orientation != Qt::Horizontal || !qBetween<int>(0, column, PoeSettingsColumn::count))
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

struct PoeSettingsTableView::Private
{
};

//--------------------------------------------------------------------------------------------------

PoeSettingsTableView::PoeSettingsTableView(QWidget* parent):
    base_type(PoeSettingsColumn::count, parent),
    d(new Private())
{
    setItemDelegate(new node_view::ResourceNodeViewItemDelegate());
    setHeaderDataProvider(kHeaderDataProvider);
    setupPoeHeader(this);
}

PoeSettingsTableView::~PoeSettingsTableView()
{
}

void PoeSettingsTableView::setupPoeHeader(QTableView* view)
{
    static constexpr int kMinimumColumnWidth = 110;

    const auto header = new QHeaderView(Qt::Horizontal, view);
    view->setHorizontalHeader(header);

    header->setSectionsClickable(true);
    header->setMinimumSectionSize(kMinimumColumnWidth);
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setSectionResizeMode(PoeSettingsColumn::port, QHeaderView::Fixed);
    header->setSectionResizeMode(PoeSettingsColumn::camera, QHeaderView::Stretch);
    header->setSectionResizeMode(PoeSettingsColumn::consumption, QHeaderView::ResizeToContents);

    header->setDefaultAlignment(Qt::AlignLeft);
}

} // namespace nx::vms::client::desktop
