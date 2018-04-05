#pragma once

#include <QtGui/QImage>
#include <QtWidgets/QWidget>


class QnAligner;

namespace Ui { class ImageControlWidget; }

namespace nx {
namespace client {
namespace desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class ImageControlWidget: public QWidget
{
    Q_OBJECT

    using base_type = QWidget;

public:
    explicit ImageControlWidget(QWidget* parent = nullptr);
    virtual ~ImageControlWidget();

    QnAligner* aligner() const;

    void setStore(CameraSettingsDialogStore* store);

private:
    void loadState(const CameraSettingsDialogState& state);

private:
    QScopedPointer<Ui::ImageControlWidget> ui;
    QnAligner* m_aligner = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx
