#ifndef ADD_CAMERA_BOOKMARK_DIALOG_H
#define ADD_CAMERA_BOOKMARK_DIALOG_H

#include <QtCore/QScopedPointer>

#include <core/resource/camera_bookmark_fwd.h>

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
class QnCameraBookmarkDialog;
}

class QnCameraBookmarkDialog : public QnButtonBoxDialog
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnCameraBookmarkDialog(QWidget *parent = 0);
    ~QnCameraBookmarkDialog();

    QnCameraBookmarkTagsUsage tagsUsage() const;
    void setTagsUsage(const QnCameraBookmarkTagsUsage &tagsUsage);

    void loadData(const QnCameraBookmark &bookmark);
    void submitData(QnCameraBookmark &bookmark) const;
private:
    void updateTagsList();

private:
    QScopedPointer<Ui::QnCameraBookmarkDialog> ui;
    QnCameraBookmarkTagsUsage m_tagsUsage;
    QStringList m_tags;
};

#endif // ADD_CAMERA_BOOKMARK_DIALOG_H
