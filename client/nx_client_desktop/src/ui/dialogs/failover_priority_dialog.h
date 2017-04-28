#pragma once

#include <client/client_color_types.h>

#include <ui/models/resource/resource_tree_model.h>

#include <ui/dialogs/resource_selection_dialog.h>

class QnFailoverPriorityResourceModelDelegate;

class QnFailoverPriorityDialog: public QnResourceSelectionDialog {
    Q_OBJECT
    typedef QnResourceSelectionDialog base_type;

    Q_PROPERTY(QnFailoverPriorityColors colors READ colors WRITE setColors)
public:
    QnFailoverPriorityDialog(QWidget* parent = nullptr);

    static QString priorityToString(Qn::FailoverPriority priority);

    const QnFailoverPriorityColors &colors() const;
    void setColors(const QnFailoverPriorityColors &colors);

protected:
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

private:
    void updatePriorityForSelectedCameras(Qn::FailoverPriority priority);

private:
    QnFailoverPriorityResourceModelDelegate* m_customColumnDelegate;
};
