// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "home_tab_button.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>

#include "private/close_tab_button.h"

namespace {
static const int kFixedIconWidth = 32;
static const int kFixedIconHeight = 32;
static const int kFixedWideHomeIconWidth = 65;

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kSystemSubstitutions =
    {{QnIcon::Normal, {.primary = "light4"}}};
static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kAddSubstitutions =
    {{QnIcon::Normal, {.primary = "dark14"}}};

NX_DECLARE_COLORIZED_ICON(kSystemIcon, "20x20/Solid/system_local.svg", kSystemSubstitutions)
NX_DECLARE_COLORIZED_ICON(kAddIcon, "20x20/Outline/add.svg", kAddSubstitutions)

} // namespace

namespace nx::vms::client::desktop {

HomeTabButton::HomeTabButton(QWidget* parent): base_type(parent)
{
    setCheckable(true);
    setIcon(qnSkin->icon(kSystemIcon));
    setFixedSize(kFixedIconWidth, kFixedIconHeight);
    setContentsMargins(1, 0, 1, 0);

    auto p = palette();
    p.setColor(QPalette::Active, QPalette::Window, core::colorTheme()->color("dark10"));
    p.setColor(QPalette::Active, QPalette::Light, core::colorTheme()->color("dark4"));
    p.setColor(QPalette::Active, QPalette::Dark, core::colorTheme()->color("dark4"));
    p.setColor(QPalette::Inactive, QPalette::Light, core::colorTheme()->color("dark8"));
    p.setColor(QPalette::Inactive, QPalette::Dark, core::colorTheme()->color("dark6"));
    setPalette(p);

    m_closeButton = new CloseTabButton(this);
    const auto layout = new QHBoxLayout(this);
    layout->addWidget(m_closeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addSpacing(10);

    connect(m_closeButton, &QAbstractButton::clicked, this,
        [this]()
        {
            if (NX_ASSERT(m_store))
                m_store->setHomeTabActive(false);
        });

    connect(this, &QAbstractButton::clicked, this, &HomeTabButton::on_clicked);
}

void HomeTabButton::setStateStore(QSharedPointer<Store> store)
{
    if (m_store)
        m_store->disconnect(this);

    m_store = store;

    if (m_store)
        connect(m_store.get(), &Store::stateChanged, this, &HomeTabButton::updateAppearance);

    updateAppearance();
}

void HomeTabButton::updateAppearance()
{
    if (!NX_ASSERT(m_store))
        return;

    const MainWindowTitleBarState& state = m_store->state();

    const bool active = state.homeTabActive;

    setChecked(active);

    if (active)
    {
        setIcon(qnSkin->icon(kSystemIcon));
        if (!state.sessions.empty())
        {
            setFixedWidth(kFixedWideHomeIconWidth);
            m_closeButton->show();
        }
        else
        {
            setFixedWidth(kFixedIconWidth);
            m_closeButton->hide();
        }
    }
    else
    {
        setIcon(qnSkin->icon(kAddIcon));
        setFixedWidth(kFixedIconWidth);
        m_closeButton->hide();
    }
}

void HomeTabButton::on_clicked()
{
    if (!NX_ASSERT(m_store))
        return;

    setChecked(true);
    m_store->setHomeTabActive(true);
}

} //namespace nx::vms::client::desktop
