#include "event_search_widget_p.h"

#include <chrono>

#include <QtWidgets/QLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>

#include <ui/models/sort_filter_list_model.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/widgets/common/search_line_edit.h>

#include <nx/client/desktop/common/models/subset_list_model.h>
#include <nx/client/desktop/event_search/models/unified_search_list_model.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static const auto kFilterDelay = std::chrono::milliseconds(250);

class EventSortFilterModel: public QnSortFilterListModel
{
public:
    using QnSortFilterListModel::QnSortFilterListModel;

    bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
    {
        return sourceLeft.data(Qn::TimestampRole).value<qint64>()
            > sourceRight.data(Qn::TimestampRole).value<qint64>();
    }
};

} // namespace

EventSearchWidget::Private::Private(EventSearchWidget* q):
    base_type(q),
    q(q),
    m_model(new UnifiedSearchListModel(this)),
    m_searchLineEdit(new QnSearchLineEdit(m_headerWidget)),
    m_typeButton(new QPushButton(m_headerWidget))
{
    m_searchLineEdit->setTextChangedSignalFilterMs(std::chrono::milliseconds(kFilterDelay).count());
    m_typeButton->setFlat(true);
    m_typeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto typeFilterMenu = new QMenu(m_headerWidget);
    typeFilterMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    typeFilterMenu->setWindowFlags(typeFilterMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    using Type = UnifiedSearchListModel::Type;
    using Types = UnifiedSearchListModel::Types;

    auto addMenuAction =
        [this, typeFilterMenu](const QString& title, const QIcon& icon, Types filter)
        {
            return typeFilterMenu->addAction(icon, title,
                [this, icon, title, filter]()
                {
                    m_typeButton->setIcon(icon);
                    m_typeButton->setText(title);
                    m_model->setFilter(filter);
                });
        };

    // TODO: #vkutin Specify icons when they're available.
    auto defaultAction = addMenuAction(tr("All types"), QIcon(), Type::all);
    addMenuAction(tr("Events"), QIcon(), Type::events);
    addMenuAction(tr("Bookmarks"), qnSkin->icon(lit("buttons/bookmark.png")), Type::bookmarks);
    addMenuAction(tr("Detected objects"), QIcon(), Type::analytics);
    defaultAction->trigger();

    m_typeButton->setMenu(typeFilterMenu);

    const auto layout = m_headerWidget->layout();
    layout->addWidget(m_searchLineEdit);
    layout->addWidget(m_typeButton);
    layout->setAlignment(m_typeButton, Qt::AlignLeft);

    auto sortModel = new EventSortFilterModel(this);
    sortModel->setSourceModel(m_model);
    sortModel->setFilterRole(Qn::ResourceSearchStringRole);
    sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    connect(m_searchLineEdit, &QnSearchLineEdit::textChanged,
        sortModel, &EventSortFilterModel::setFilterWildcard);

    setModel(new SubsetListModel(sortModel, 0, QModelIndex(), this));
}

EventSearchWidget::Private::~Private()
{
}

QnVirtualCameraResourcePtr EventSearchWidget::Private::camera() const
{
    return m_model->camera();
}

void EventSearchWidget::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    m_model->setCamera(camera);
}
} // namespace
} // namespace client
} // namespace nx
