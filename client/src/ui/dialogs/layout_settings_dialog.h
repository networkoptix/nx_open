#ifndef LAYOUT_SETTINGS_DIALOG_H
#define LAYOUT_SETTINGS_DIALOG_H

#include <QtGui/QDialog>
#include <QtGui/QLabel>

#include <core/resource/resource_fwd.h>

namespace Ui {
    class QnLayoutSettingsDialog;
}

class QnAppServerImageCache;
class QnLayoutSettingsDialogPrivate;

class QnLayoutSettingsDialog : public QDialog
{
    Q_OBJECT
    typedef QDialog base_type;

public:
    explicit QnLayoutSettingsDialog(QWidget *parent = 0);
    ~QnLayoutSettingsDialog();

    void readFromResource(const QnLayoutResourcePtr &layout);
    bool submitToResource(const QnLayoutResourcePtr &layout);

protected:
    virtual bool eventFilter(QObject *target, QEvent *event) override;
private slots:
    void at_clearButton_clicked();
    void at_accepted();
    void at_opacitySpinBox_valueChanged(int value);

    void at_imageLoaded(const QString& filename, bool ok);
    void at_imageStored(const QString& filename, bool ok);

    void setPreview(const QImage& image);
    void setProgress(bool value);

    void updateControls();

    void viewFile();

    void selectFile();
private:
    qreal screenAspectRatio() const;

    bool hasChanges(const QnLayoutResourcePtr &layout);

    void loadPreview();

    Q_DECLARE_PRIVATE(QnLayoutSettingsDialog)
private:
    QScopedPointer<Ui::QnLayoutSettingsDialog> ui;
    QnLayoutSettingsDialogPrivate *const d_ptr;

    QnAppServerImageCache *m_cache;

    bool m_isUpdating;
};

#endif // LAYOUT_SETTINGS_DIALOG_H
