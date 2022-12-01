// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("QtCore/QItemSelectionModel")

class QItemSelectionModel;

namespace nx::vms::client::desktop {

/**
 * This helper class provides navigation and selection in list views similar to
 * QAbstractItemView::ExtendedSelection mode by operating over a provided item selection model.
 *
 * Mouse events should be translated to ListNavigationHelper::navigateTo calls,
 * and key events should be translated to ListNavigationHelper::navigate calls.
 */
class NX_VMS_CLIENT_DESKTOP_API ListNavigationHelper: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QItemSelectionModel* selectionModel READ selectionModel WRITE setSelectionModel
        NOTIFY selectionModelChanged)
    Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY pageSizeChanged)

    using base_type = QObject;

public:
    ListNavigationHelper(QObject* parent = nullptr);
    virtual ~ListNavigationHelper() override;

    QItemSelectionModel* selectionModel() const;
    void setSelectionModel(QItemSelectionModel* value);

    int pageSize() const; //< Items per page.
    void setPageSize(int value);

    enum class Move
    {
        up,
        down,
        pageUp,
        pageDown,
        home,
        end
    };
    Q_ENUM(Move)

    Q_INVOKABLE void navigate(Move type, Qt::KeyboardModifiers modifiers);
    Q_INVOKABLE void navigateTo(const QModelIndex& index, Qt::KeyboardModifiers modifiers);

    static void registerQmlType();

signals:
    void selectionModelChanged();
    void pageSizeChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
