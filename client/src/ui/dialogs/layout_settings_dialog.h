#ifndef LAYOUT_SETTINGS_DIALOG_H
#define LAYOUT_SETTINGS_DIALOG_H

#include <QtGui/QDialog>
#include <QtGui/QLabel>

#include <core/resource/resource_fwd.h>

#include <utils/app_server_file_cache.h>

namespace Ui {
    class QnLayoutSettingsDialog;
}

class QnFramedLabel;

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
private slots:
    void at_viewButton_clicked();
    void at_selectButton_clicked();
    void at_clearButton_clicked();
    void at_accepted();
    void at_opacitySpinBox_valueChanged(int value);

    void at_imageLoaded(const QString& filename, bool ok);
    void at_imageStored(const QString& filename, bool ok);

    void setPreview(const QImage& image);
    void setProgress(bool value);

    void updateControls();
private:
    bool hasChanges(const QnLayoutResourcePtr &layout);

    void loadPreview();

private:
    QScopedPointer<Ui::QnLayoutSettingsDialog> ui;
    QnAppServerFileCache *m_cache;
    QnFramedLabel* imageLabel;

    QString m_cachedFilename;
    QString m_newFilePath;

    qreal m_cellAspectRatio;
    bool m_estimatePending;
};

#endif // LAYOUT_SETTINGS_DIALOG_H
