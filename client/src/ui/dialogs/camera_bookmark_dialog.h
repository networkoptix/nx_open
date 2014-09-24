#ifndef ADD_CAMERA_BOOKMARK_DIALOG_H
#define ADD_CAMERA_BOOKMARK_DIALOG_H

#include <QtCore/QScopedPointer>

#include <core/resource/camera_bookmark_fwd.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
class QnCameraBookmarkDialog;
}

class QnCameraBookmarkDialog : public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    explicit QnCameraBookmarkDialog(QWidget *parent = 0);
    ~QnCameraBookmarkDialog();

    QnCameraBookmarkTags tags() const;
    void setTags(const QnCameraBookmarkTags &tags);

    void loadData(const QnCameraBookmark &bookmark);
    void submitData(QnCameraBookmark &bookmark) const;
private:
    void updateTagsList();

private:
    QScopedPointer<Ui::QnCameraBookmarkDialog> ui;
    QnCameraBookmarkTags m_allTags;
    QnCameraBookmarkTags m_selectedTags;
};

#endif // ADD_CAMERA_BOOKMARK_DIALOG_H
