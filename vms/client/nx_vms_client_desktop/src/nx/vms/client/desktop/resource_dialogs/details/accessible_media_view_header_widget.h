// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtWidgets/QWidget>

namespace Ui { class AccessibleMediaViewHeaderWidget; }

class QStandardItemModel;
class QStandardItem;
class QnIndents;

namespace nx::vms::client::desktop {

/**
 * Widget which contains single "All Cameras & Resources" checkbox stylized by checkable row
 * from the resource tree. Used as header for the resource view widget in the accessible media
 * selection tab for custom user user role.
 */
class AccessibleMediaViewHeaderWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    AccessibleMediaViewHeaderWidget(QWidget* parent = nullptr);
    virtual ~AccessibleMediaViewHeaderWidget() override;

    /**
     * Current checked state getter.
     */
    bool allMediaCheked();

    /**
     * Checked state setter, explicitly applies new value and doesn't spawn any notifications or
     * side effects.
     */
    void setAllMediaCheked(bool checked);

signals:
    /**
     * Provided checkable row isn't user intractable, that is to say that its transitions of the
     * state are responsibility of the user. Signal emitted when user clicks checkbox, it doesn't
     * imply that checked state is changed.
     */
    void allMediaCheckableItemClicked();

private:
    const std::unique_ptr<Ui::AccessibleMediaViewHeaderWidget> ui;
    const std::unique_ptr<QStandardItemModel> m_allMediaCheckableItemModel;
    QStandardItem* m_allMediaCheckableItem;
};

} // namespace nx::vms::client::desktop
