#pragma once

#include <QtWidgets/QWidget>

namespace Ui { class ArchiveLengthWidget; }
class QnDisconnectHelper;

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class Aligner;

class ArchiveLengthWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit ArchiveLengthWidget(QWidget* parent = nullptr);
    virtual ~ArchiveLengthWidget();

    Aligner* aligner() const;

    void setStore(CameraSettingsDialogStore* store);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    const QScopedPointer<Ui::ArchiveLengthWidget> ui;
    QScopedPointer<QnDisconnectHelper> m_storeConnections;
    Aligner* const m_aligner = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx
