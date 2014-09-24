#ifndef LAYOUT_SETTINGS_DIALOG_H
#define LAYOUT_SETTINGS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
    class LayoutSettingsDialog;
}

class QnAppServerImageCache;
class QnLayoutSettingsDialogPrivate;

class QnLayoutSettingsDialog : public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    explicit QnLayoutSettingsDialog(QWidget *parent = 0);
    ~QnLayoutSettingsDialog();

    void readFromResource(const QnLayoutResourcePtr &layout);
    bool submitToResource(const QnLayoutResourcePtr &layout);

    virtual void accept() override;

protected:
    virtual bool eventFilter(QObject *target, QEvent *event) override;

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
    /** Aspect ratio of the current screen. */
    qreal screenAspectRatio() const;

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

    bool hasChanges(const QnLayoutResourcePtr &layout);

    void loadPreview();

    Q_DECLARE_PRIVATE(QnLayoutSettingsDialog)

private:
    QScopedPointer<Ui::LayoutSettingsDialog> ui;
    QnLayoutSettingsDialogPrivate *const d_ptr;

    QnAppServerImageCache *m_cache;

    bool m_isUpdating;
};

#endif // LAYOUT_SETTINGS_DIALOG_H
