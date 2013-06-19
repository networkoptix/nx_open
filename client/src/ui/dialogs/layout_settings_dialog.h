#ifndef LAYOUT_SETTINGS_DIALOG_H
#define LAYOUT_SETTINGS_DIALOG_H

#include <QtGui/QDialog>
#include <QtGui/QLabel>

#include <core/resource/resource_fwd.h>

namespace Ui {
    class QnLayoutSettingsDialog;
}

class QnFramedLabel;
class QnAppServerImageCache;

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
    virtual void showEvent(QShowEvent *event) override;
    virtual void resizeEvent(QResizeEvent *event) override;

    virtual bool eventFilter(QObject *target, QEvent *event) override;
private slots:
    void at_clearButton_clicked();
    void at_accepted();
    void at_opacitySpinBox_valueChanged(int value);

    void at_imageLoaded(const QString& filename, bool ok);
    void at_imageStored(const QString& filename, bool ok);

    void setPreview(const QImage& image);
    void setProgress(bool value);

    void updateCache(bool local);

    void updateControls();

    void viewFile();
    void selectFile();
private:
    bool hasChanges(const QnLayoutResourcePtr &layout);

    void loadPreview();
private:
    QScopedPointer<Ui::QnLayoutSettingsDialog> ui;
    QnAppServerImageCache *m_cache;
    QnFramedLabel* imageLabel;

    QString m_cachedFilename;
    QString m_newFilePath;

    qreal m_cellAspectRatio;
    bool m_estimatePending;

    /** Should image be cropped to current monitor AR */
    bool m_cropImage;
};

#endif // LAYOUT_SETTINGS_DIALOG_H
