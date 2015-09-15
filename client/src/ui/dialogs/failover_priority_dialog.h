#pragma once

#include <ui/dialogs/resource_selection_dialog.h>

class QnFailoverPriorityDialog: public QnResourceSelectionDialog {
    Q_OBJECT
    typedef QnResourceSelectionDialog base_type;

public:
    QnFailoverPriorityDialog(QWidget* parent = nullptr);

    static QString priorityToString(Qn::FailoverPriority priority);
private:
    void updatePriorityForSelectedCameras(Qn::FailoverPriority priority);
    
};
