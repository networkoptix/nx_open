// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/camera_bookmark_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class QnCameraBookmarkDialog; }

class QnCameraBookmarkDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    enum class Mode
    {
        create,
        edit
    };

    explicit QnCameraBookmarkDialog(Mode mode, bool mandatoryDescription, QWidget* parent);
    virtual ~QnCameraBookmarkDialog() override;

    const QnCameraBookmarkTagList& tags() const;
    void setTags(const QnCameraBookmarkTagList& tags);

    void loadData(const QnCameraBookmark& bookmark);
    void submitData(QnCameraBookmark& bookmark) const;
    bool sharingRequested() const;

    virtual void accept() override;

protected:
    virtual void initializeButtonBox() override;
    virtual void buttonBoxClicked(QAbstractButton* button) override;

private:
    void updateAcceptButtonsAvailability();

private:
    struct Private;
    nx::utils::ImplPtr<Ui::QnCameraBookmarkDialog> ui;
    nx::utils::ImplPtr<Private> d;
};
