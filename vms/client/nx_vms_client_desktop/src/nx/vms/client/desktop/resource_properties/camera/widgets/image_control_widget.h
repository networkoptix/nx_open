#pragma once

#include <QtGui/QImage>
#include <QtWidgets/QWidget>

namespace Ui { class ImageControlWidget; }

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class Aligner;

class ImageControlWidget: public QWidget
{
    Q_OBJECT

    using base_type = QWidget;

public:
    explicit ImageControlWidget(QWidget* parent = nullptr);
    virtual ~ImageControlWidget() override;

    Aligner* aligner() const;

    void setStore(CameraSettingsDialogStore* store);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    QScopedPointer<Ui::ImageControlWidget> ui;
    Aligner* m_aligner = nullptr;
};

} // namespace nx::vms::client::desktop
