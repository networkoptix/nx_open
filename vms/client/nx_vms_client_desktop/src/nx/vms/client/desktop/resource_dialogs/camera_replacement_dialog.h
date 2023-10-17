// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class CameraReplacementDialog; }

namespace nx::vms::client::desktop {

class CameraReplacementDialog: public QnSessionAwareDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareDialog;

public:

    /**
     * @param cameraToBeReplaced Camera that should be replaced with another one selected by user.
     *     It must meet the conditions for the camera to be replaced, see
     *     <tt>nx::vms::common::utils::camera_replacement::cameraCanBeReplaced()</tt>.
     * @param parent Pointer to the parent widget, should be both a window and an instance of class
     *     derived from <tt>QnWorkbenchContextAware</tt>.
     */
    CameraReplacementDialog(
        const QnVirtualCameraResourcePtr& cameraToBeReplaced,
        QWidget* parent);

    virtual ~CameraReplacementDialog() override;

    /**
     * @return True if there are no suitable cameras to choose as replacement devices. There is no
     *     point to show the dialog in that case.
     */
    bool isEmpty() const;

    enum DialogState
    {
        InvalidState = -1,
        CameraSelection,
        ReplacementApproval,
        ReplacementSummary,
    };

    /**
     * @return The current state of the dialog. As the dialog itself is a three step wizard, each
     *     state represents one of the dialog pages: device selection page, replacement approval
     *     page where possible problems will be reported, and replacement summary page reporting
     *     on the successfulness of the replacement operation. Fourth case, <tt>InvalidState</tt>
     *     may be returned only if dialog was initialized with invalid data, e.g null pointer to
     *     the camera resource was passed to the constructor, in such case dialog will immediately
     *     rejected on the show attempt.
     */
    DialogState dialogState() const;

public slots:
    virtual int	exec() override;

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private:
    void onNextButtonClicked();
    void onBackButtonClicked();
    void makeReplacementRequest(bool getReportOnly);

    void setupUiContols();
    void updateDataTransferReportPage();
    void updateReplacementSummaryPage();
    void updateButtons();
    void updateHeader();

private:
    struct Private;
    nx::utils::ImplPtr<Ui::CameraReplacementDialog> ui;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
