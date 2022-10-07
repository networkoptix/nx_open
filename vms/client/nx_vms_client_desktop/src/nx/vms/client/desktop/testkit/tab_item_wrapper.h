// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QVariant>
#include <QtWidgets/QTabBar>

namespace nx::vms::client::desktop::testkit {

/**
 * Wrapper class for tabs in QTabBar. It also exposes properties of tabs to scripting.
 */
class TabItemWrapper
{
    Q_GADGET
    Q_PROPERTY(QString text READ text)
    Q_PROPERTY(QString type READ type)
    Q_PROPERTY(QString toolTip READ toolTip)
    Q_PROPERTY(QVariant data READ data)
    Q_PROPERTY(bool enabled READ enabled)
    Q_PROPERTY(bool visible READ visible)

public:
    TabItemWrapper(int index = -1, QTabBar* container = nullptr):
        m_index(index),
        m_container(container)
    {
    }

    static QString type() { return "TabItem"; }

    QString text() const { return m_container->tabText(m_index); }
    QString toolTip() const { return m_container->tabToolTip(m_index); }

    QVariant data() const { return m_container->tabData(m_index); }

    QTabBar* container() const { return m_container; }

    bool isValid() const { return m_index >= 0 && m_container; }

    int index() const { return m_index; }

    bool enabled() const { return m_container->isTabEnabled(m_index); }
    bool visible() const { return m_container->isTabVisible(m_index); }

    QWidget* leftButton() const { return m_container->tabButton(m_index, QTabBar::LeftSide); }
    QWidget* rightButton() const { return m_container->tabButton(m_index, QTabBar::RightSide); }

private:
    int m_index;
    QPointer<QTabBar> m_container;
};

} // namespace nx::vms::client::desktop::testkit
