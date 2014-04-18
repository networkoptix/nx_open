#ifndef ADD_CAMERA_BOOKMARK_DIALOG_H
#define ADD_CAMERA_BOOKMARK_DIALOG_H

#include <QtCore/QScopedPointer>

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
class QnAddCameraBookmarkDialog;
}

class QnAddCameraBookmarkDialog : public QnButtonBoxDialog
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnAddCameraBookmarkDialog(QWidget *parent = 0);
    ~QnAddCameraBookmarkDialog();

private:
    void updateTagsList();

private:
    QScopedPointer<Ui::QnAddCameraBookmarkDialog> ui;
};

#endif // ADD_CAMERA_BOOKMARK_DIALOG_H
