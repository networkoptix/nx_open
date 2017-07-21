#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
class ExportRapidReviewDialog;
}

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace dialogs {

class ExportRapidReviewPrivate;

class ExportRapidReview: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

    typedef QnSessionAwareButtonBoxDialog base_type;
public:
    explicit ExportRapidReview(QWidget* parent = nullptr,
        Qt::WindowFlags windowFlags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    virtual ~ExportRapidReview();

    static qint64 kMinimalSourcePeriodLength;
    qint64 sourcePeriodLengthMs() const;
    void setSourcePeriodLengthMs(qint64 lengthMs);

    void setSpeed(qint64 value);
    qint64 speed() const;

    qint64 frameStepMs() const;

    /* Magic knowledge. We know that the result will be generated with 30 fps. --rvasilenko */
    static const int kResultFps = 30;

private:
    void updateRanges();
    void updateControls();

private:
     Q_DECLARE_PRIVATE(ExportRapidReview)
     QScopedPointer<ExportRapidReviewPrivate> d_ptr;
     QScopedPointer<Ui::ExportRapidReviewDialog> ui;
     bool m_updating;
};

} // namespace dialogs
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
