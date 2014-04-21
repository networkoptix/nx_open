#ifndef ADD_CAMERA_BOOKMARK_DIALOG_H
#define ADD_CAMERA_BOOKMARK_DIALOG_H

#include <QtCore/QScopedPointer>

#include <core/resource/camera_bookmark.h>

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

    QHash<QString, int> tagsUsage() const;
    void setTagsUsage(const QHash<QString, int> tagsUsage);

    void loadData(const QnCameraBookmark &bookmark);
    void submitData(QnCameraBookmark &bookmark) const;
private:
    void updateTagsList();

private:
    QScopedPointer<Ui::QnCameraBookmarkDialog> ui;
    QHash<QString, int> m_tagsUsage;
    QStringList m_tags;
};

#endif // ADD_CAMERA_BOOKMARK_DIALOG_H
