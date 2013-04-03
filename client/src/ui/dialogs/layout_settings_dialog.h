#ifndef LAYOUT_SETTINGS_DIALOG_H
#define LAYOUT_SETTINGS_DIALOG_H

#include <QDialog>

#include <core/resource/resource_fwd.h>

#include <utils/app_server_file_cache.h>

namespace Ui {
    class QnLayoutSettingsDialog;
}

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

    void at_image_loaded(int id);
    void at_image_stored(int id);

    void setPreview(const QImage& image);
    void setProgress(bool value);

    void updateControls();
private:
    bool hasChanges(const QnLayoutResourcePtr &layout);

    void loadPreview();

private:
    QScopedPointer<Ui::QnLayoutSettingsDialog> ui;
    QnAppServerFileCache *m_cache;

    QString m_filename;
    int m_layoutImageId;
    qreal m_cellAspectRatio;
    bool m_estimatePending;
};

#endif // LAYOUT_SETTINGS_DIALOG_H
