// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QPointer>

class QTreeView;
class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class ResourceTreeInteractionHandler;

/**
* Decorator class which provides Resource Tree context menu for the given tree view.
*/
class ResourceTreeContextMenuExtension: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit ResourceTreeContextMenuExtension(QTreeView* treeView,
        QnWorkbenchContext* workbenchContext);
    virtual ~ResourceTreeContextMenuExtension() override;

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QPointer<QTreeView> m_treeView;
    const std::unique_ptr<ResourceTreeInteractionHandler> m_interactionHandler;
};

} // namespace nx::vms::client::desktop
