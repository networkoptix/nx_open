#pragma once

#include <functional>

#include <QtCore/QScopedPointer>
#include <QtGui/QValidator>
#include <QtWidgets/QStyledItemDelegate>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/customization/customized.h>
#include <ui/utils/validators.h>

class QnResourceListModel;
class QnUuid;

namespace Ui {
class LayoutSelectionDialog;
} // namespace Ui

namespace nx {
namespace client {
namespace desktop {
namespace ui {

// A dialog to select one layout.
// Used in OpelLayoutActionWidget.
class LayoutSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit LayoutSelectionDialog(bool singlePick, QWidget* parent, Qt::WindowFlags windowFlags = 0);
    virtual ~LayoutSelectionDialog() override;

    // Mode for selecting local layout
    enum LocalLayoutSelection
    {
        ModeFull,       //< Can select any layout.
        ModeLimited,    //< Show selected layout, but clear selection if user picks shared one
        ModeHideLocal,      //< No local layouts are displayed
    };

    // Shows alert with specified text. If text is empty, alert is hidden.
    void showAlert(const QString& text);

    // Shows info with specified text. If text is empty, widget is hidden.
    void showInfo(const QString& text);

    // Invalid ids are filtered out. Disabled users are kept, but hidden.
    void setLocalLayouts(const QnResourceList& layouts,
        const QSet<QnUuid>& selection, LocalLayoutSelection mode);

    void setSharedLayouts(const QnResourceList& layouts, const QSet<QnUuid>& selection);

    // Explicitly checked subjects, regardless of allUsers value.
    QSet<QnUuid> checkedLayouts() const;

signals:
    void layoutsChanged(); //< Selection or contents were changed. Potential alert must be re-evaluated.

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
    bool m_singlePick = true;

    LocalLayoutSelection m_localSelectionMode;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
