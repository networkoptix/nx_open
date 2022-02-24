// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QMenu>

#include <nx/utils/impl_ptr.h>

class QAbstractItemModel;
class QAbstractItemView;
class QModelIndex;

namespace nx::vms::client::desktop {

class ItemModelMenu: public QMenu
{
    Q_OBJECT
    using base_type = QMenu;

public:
    explicit ItemModelMenu(QWidget* parent = nullptr);
    virtual ~ItemModelMenu() override;

    QAbstractItemModel* model() const;
    void setModel(QAbstractItemModel* value);

    QAbstractItemView* view() const;
    void setView(QAbstractItemView* value);

    virtual QSize sizeHint() const override;

signals:
    void itemHovered(const QModelIndex& index);
    void itemTriggered(const QModelIndex& index);

protected:
    virtual bool event(QEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
