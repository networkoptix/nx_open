#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class LayoutSettingsDialog; }

namespace nx {
namespace client {
namespace desktop {

class ServerImageCache;

} // namespace desktop
} // namespace client
} // namespace nx

class QnLayoutSettingsDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit QnLayoutSettingsDialog(QWidget* parent = nullptr);
    virtual ~QnLayoutSettingsDialog() override;

    void readFromResource(const QnLayoutResourcePtr& layout);
    bool submitToResource(const QnLayoutResourcePtr& layout);

    virtual void accept() override;

protected:
    virtual bool eventFilter(QObject* target, QEvent* event) override;

private slots:
    void at_clearButton_clicked();
    void at_opacitySpinBox_valueChanged(int value);
    void at_widthSpinBox_valueChanged(int value);
    void at_heightSpinBox_valueChanged(int value);

    void at_imageLoaded(const QString& filename, bool ok);
    void at_imageStored(const QString& filename, bool ok);

    void setPreview(const QImage& image);
    void setProgress(bool value);

    void updateControls();

    void viewFile();

    void selectFile();

private:
    /**
     * Aspect ratio that is optimal for cells to best fit the current image.
     * Returns negative value if image is not available.
     */
    qreal bestAspectRatioForCells() const;

    /**
     * Returns true if width and height in cells are already set to values
     * corresponding to bestAspectRatioForCells()
     */
    bool cellsAreBestAspected() const;

    bool hasChanges(const QnLayoutResourcePtr& layout);

    void loadPreview();

private:
    struct Private;
    QScopedPointer<Ui::LayoutSettingsDialog> ui;
    QScopedPointer<Private> d;

    nx::client::desktop::ServerImageCache* m_cache;

    bool m_isUpdating;
};
