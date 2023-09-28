// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fake_resource_list_view.h"

#include <QtWidgets/QHeaderView>

#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/indents.h>

namespace {

static constexpr qsizetype kMaximumRows = 10;
static constexpr int kRecommendedWidth = 284;

} // namespace

namespace nx::vms::client::desktop {

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
    setProperty(style::Properties::kSideIndentation, QVariant::fromValue(QnIndents(0, 0)));
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

} // namespace nx::vms::client::desktop
