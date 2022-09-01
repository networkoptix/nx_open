// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QTabBar>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/ui/actions/action_target_provider.h>
#include <ui/workbench/workbench_context_aware.h>

class QnLayoutTabBar:
    public QTabBar,
    public QnWorkbenchContextAware,
    public nx::vms::client::desktop::ui::action::TargetProvider
{
    Q_OBJECT
    typedef QTabBar base_type;

public:
    QnLayoutTabBar(QWidget* parent = nullptr);

    virtual ~QnLayoutTabBar();

    virtual nx::vms::client::desktop::ui::action::ActionScope currentScope() const override;
    virtual nx::vms::client::desktop::ui::action::Parameters currentParameters(
        nx::vms::client::desktop::ui::action::ActionScope scope) const override;

signals:
    void tabTextChanged();

protected:
    virtual QSize minimumSizeHint() const override;
    virtual QSize tabSizeHint(int index) const override;
    virtual QSize minimumTabSizeHint(int index) const override;

    virtual void contextMenuEvent(QContextMenuEvent *event) override;

    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;

    virtual void showEvent(QShowEvent* event) override;

    virtual void tabInserted(int index) override;
    virtual void tabRemoved(int index) override;

    QString layoutText(QnWorkbenchLayout *layout) const;
    QIcon layoutIcon(QnWorkbenchLayout *layout) const;

    void updateTabText(QnWorkbenchLayout *layout);
    void updateTabIcon(QnWorkbenchLayout *layout);

private:
    void at_currentChanged(int index);
    void at_tabMoved(int from, int to);

    void at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource);

    void at_workbench_layoutsChanged();
    void at_workbench_currentLayoutChanged();

private:
    void checkInvariants() const;
    void submitCurrentLayout();
    void fixGeometry();

private:
    /** Whether changes to tab bar should be written back into the workbench. */
    bool m_submit;

    /** Whether changes to the workbench should be written into this tab bar. */
    bool m_update;

    /** Tab-to-layout mapping. */
    QList<QnWorkbenchLayout *> m_layouts;

    /** Tab we are trying to close with middle mouse button */
    int m_midClickedTab;
};
