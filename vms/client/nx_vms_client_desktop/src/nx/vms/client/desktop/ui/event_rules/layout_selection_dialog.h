// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QScopedPointer>
#include <QtGui/QValidator>
#include <QtWidgets/QStyledItemDelegate>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/utils/uuid.h>

class QnResourceListModel;

namespace Ui {
class LayoutSelectionDialog;
} // namespace Ui

namespace nx::vms::client::desktop {

// A dialog to select one or several layouts.
class LayoutSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit LayoutSelectionDialog(
        bool singlePick,
        QWidget* parent = nullptr,
        Qt::WindowFlags windowFlags = {});
    virtual ~LayoutSelectionDialog() override;

    // Mode for selecting local layout.
    enum LocalLayoutSelection
    {
        // Can select any layout.
        ModeFull,
        // Show selected layout, but clear selection if user picks shared one.
        ModeLimited,
        // No local layouts are displayed.
        ModeHideLocal,
    };

    // Shows alert with specified text. If text is empty, alert is hidden.
    void showAlert(const QString& text);

    // Shows info with specified text. If text is empty, widget is hidden.
    void showInfo(const QString& text);

    // Invalid ids are filtered out. Disabled users are kept, but hidden.
    void setLocalLayouts(const QnResourceList& layouts,
        const QSet<nx::Uuid>& selection, LocalLayoutSelection mode);

    void setSharedLayouts(const QnResourceList& layouts, const QSet<nx::Uuid>& selection);

    // Explicitly checked subjects, regardless of allUsers value.
    QSet<nx::Uuid> checkedLayouts() const;

signals:
    // Selection or contents were changed. Potential alert must be re-evaluated.
    void layoutsChanged();

private:
    void at_localLayoutSelected();
    void at_sharedLayoutSelected();
    void at_layoutsChanged();

private:
    QScopedPointer<Ui::LayoutSelectionDialog> ui;
    // A model to display local layouts.
    QPointer<QnResourceListModel> m_localLayoutsModel;
    // A model to display shared layouts.
    QPointer<QnResourceListModel> m_sharedLayoutsModel;
    const bool m_singlePick;

    LocalLayoutSelection m_localSelectionMode = ModeFull;
};

} // namespace nx::vms::client::desktop
