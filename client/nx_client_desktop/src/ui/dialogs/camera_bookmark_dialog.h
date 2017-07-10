#ifndef ADD_CAMERA_BOOKMARK_DIALOG_H
#define ADD_CAMERA_BOOKMARK_DIALOG_H

#include <QtCore/QScopedPointer>

#include <nx/vms/event/event_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
class QnCameraBookmarkDialog;
}

class QnCameraBookmarkDialog : public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

    typedef QnSessionAwareButtonBoxDialog base_type;

public:
    explicit QnCameraBookmarkDialog(QWidget *parent = 0);

    QnCameraBookmarkDialog(
        const nx::vms::event::AbstractActionPtr& action,
        QWidget* parent = nullptr);

    ~QnCameraBookmarkDialog();

    const QnCameraBookmarkTagList &tags() const;
    void setTags(const QnCameraBookmarkTagList &tags);

    void loadData(const QnCameraBookmark &bookmark);
    void submitData(QnCameraBookmark &bookmark) const;

    virtual void accept() override;

    QnCameraBookmarkList bookmarks() const;

protected:
    virtual void initializeButtonBox() override;

private:
    void initialize();

    void updateOkButtonAvailability();\

private:
    QScopedPointer<Ui::QnCameraBookmarkDialog> ui;
    QnCameraBookmarkList m_actionBookmarks;
};

#endif // ADD_CAMERA_BOOKMARK_DIALOG_H
