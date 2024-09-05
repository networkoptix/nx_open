// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "home_tab_button.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMenu>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/window_context.h>

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
    auto layout = new QHBoxLayout();
    layout->addWidget(m_closeButton, 0, Qt::AlignRight | Qt::AlignVCenter);
    setLayout(layout);

    connect(m_closeButton, &QAbstractButton::clicked, this,
        [this]()
        {
            if (!NX_ASSERT(m_store))
                return;

            const auto state = m_store->state();
            const auto systemIndex =
                state.activeSystemTab >= 0 && state.activeSystemTab < state.systems.size()
                ? state.activeSystemTab
                : (state.systems.size() - 1);

            if (systemIndex >= 0)
            {
                const auto systemData = state.systems[systemIndex];
                if (systemData.systemDescription->localId() == state.currentSystemId)
                {
                    appContext()->mainWindowContext()->menu()->action(menu::ResourcesModeAction)->
                        setChecked(true);
                }
                else
                {
                    m_stateHandler->connectToSystem(
                        systemData.systemDescription, systemData.logonData);
                }
            }
        });

    connect(this, &QAbstractButton::clicked, this, &HomeTabButton::on_clicked);
}

void HomeTabButton::setStateStore(QSharedPointer<Store> store,
    QSharedPointer<StateHandler> stateHandler)
{
    if (m_stateHandler)
        m_stateHandler->disconnect(this);
    m_stateHandler = stateHandler;
    m_store = store;
    if (m_stateHandler)
    {
        connect(
            m_stateHandler.get(),
            &SystemTabBarStateHandler::homeTabActiveChanged,
            this,
            &HomeTabButton::updateAppearance);
        connect(
            m_stateHandler.get(),
            &SystemTabBarStateHandler::tabsChanged,
            this,
            &HomeTabButton::updateAppearance);
        if (m_store)
            updateAppearance();
    }
}

void HomeTabButton::updateAppearance()
{
    if (!NX_ASSERT(m_store))
        return;

    bool active = m_store->isHomeTabActive();
    setChecked(active);
    if (active)
    {
        setIcon(qnSkin->icon(kSystemIcon));
        if (m_store->systemCount() > 0)
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

    appContext()->mainWindowContext()->menu()->action(menu::ResourcesModeAction)->setChecked(false);
    setChecked(true);
}

} //namespace nx::vms::client::desktop
