#include "fake_resource_list_view.h"

#include <QtWidgets/QHeaderView>

#include <ui/style/helper.h>
#include <ui/common/indents.h>

namespace {

static constexpr int kMaximumRows = 10;
static constexpr int kRecommendedWidth = 284;

} // namespace

namespace nx {
namespace client {
namespace desktop {

FakeResourceListView::FakeResourceListView(
    const FakeResourceList& fakeResources,
    QWidget* parent)
    :
    base_type(),
    m_model(new FakeResourceListModel(fakeResources, this))
{
    setModel(m_model);

    setRootIsDecorated(false);
    setUniformRowHeights(true);
    setHeaderHidden(true);
    setFocusPolicy(Qt::NoFocus);
    setProperty(style::Properties::kSuppressHoverPropery, true);
    setProperty(style::Properties::kSideIndentation, qVariantFromValue(QnIndents(0, 0)));
    setSelectionMode(QAbstractItemView::NoSelection);

    const auto horizontalHeader = header();
    horizontalHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
    horizontalHeader->setStretchLastSection(true);
}

FakeResourceListView::~FakeResourceListView()
{
}

QSize FakeResourceListView::sizeHint() const
{
    const auto height = style::Metrics::kViewRowHeight
        * std::min(kMaximumRows, m_model->resources().size());
    return QSize(kRecommendedWidth, height);
}

} // namespace desktop
} // namespace client
} // namespace nx

