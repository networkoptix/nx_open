#ifndef ADD_CAMERA_BOOKMARK_DIALOG_H
#define ADD_CAMERA_BOOKMARK_DIALOG_H

#include <QtCore/QScopedPointer>

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
    explicit QnCameraBookmarkDialog(bool mandatoryDescription, QWidget *parent);

    ~QnCameraBookmarkDialog();

    const QnCameraBookmarkTagList &tags() const;
    void setTags(const QnCameraBookmarkTagList &tags);

    void loadData(const QnCameraBookmark &bookmark);
    void submitData(QnCameraBookmark &bookmark) const;

    virtual void accept() override;

protected:
    virtual void initializeButtonBox() override;

private:
    void updateOkButtonAvailability();

private:
    QScopedPointer<Ui::QnCameraBookmarkDialog> ui;
};

#endif // ADD_CAMERA_BOOKMARK_DIALOG_H
