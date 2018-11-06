#pragma once

#include <ui/delegates/resource_selection_dialog_delegate.h>

namespace nx::vms::client::desktop {

class ExportScheduleResourceSelectionDialogDelegate: public QnResourceSelectionDialogDelegate
{
    Q_OBJECT
    using base_type = QnResourceSelectionDialogDelegate;

public:
    ExportScheduleResourceSelectionDialogDelegate(
        QWidget* parent,
        bool recordingEnabled,
        bool motionUsed,
        bool dualStreamingUsed,
        bool hasVideo);

    bool doCopyArchiveLength() const;

    virtual void init(QWidget* parent) override;

    virtual bool validate(const QSet<QnUuid>& selected) override;

private:
    QLabel* m_licensesLabel = nullptr;
    QLabel* m_motionLabel = nullptr;
    QLabel* m_dtsLabel = nullptr;
    QLabel* m_noVideoLabel = nullptr;
    QCheckBox *m_archiveCheckbox = nullptr;
    QPalette m_parentPalette;

    const bool m_recordingEnabled;
    const bool m_motionUsed;
    const bool m_dualStreamingUsed;
    const bool m_hasVideo;
};

} // namespace nx::vms::client::desktop
