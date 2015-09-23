
#pragma once

#include <QDialog>

/// @brief Base dialog class. Fixes dialog hang on when dragging items on show.
/// Cancels drag event before showing dialog.
class QnDialog : public QDialog
{
public:
    QnDialog(QWidget * parent = nullptr, Qt::WindowFlags flags = 0);

    /// @brief Shows dialog and cancels drag action to prevent hang on
    static void show(QDialog *dialog);

    /// @brief Shows dialog and cancels drag action to prevent hang on
    void show();
};
