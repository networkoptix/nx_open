#ifndef ADD_CAMERA_BOOKMARK_DIALOG_H
#define ADD_CAMERA_BOOKMARK_DIALOG_H

#include <QtCore/QScopedPointer>

#include <core/resource/camera_bookmark.h>

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

    QHash<QString, int> tagsUsage() const;
    void setTagsUsage(const QHash<QString, int> tagsUsage);

    void loadData(const QnCameraBookmark &bookmark);
    void submitData(QnCameraBookmark &bookmark) const;
private:
    void updateTagsList();

private:
    QScopedPointer<Ui::QnAddCameraBookmarkDialog> ui;
    QHash<QString, int> m_tagsUsage;
    QStringList m_tags;
};

#endif // ADD_CAMERA_BOOKMARK_DIALOG_H
