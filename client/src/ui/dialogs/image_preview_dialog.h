#ifndef IMAGE_PREVIEW_DIALOG_H
#define IMAGE_PREVIEW_DIALOG_H

#include <QtWidgets/QDialog>

namespace Ui {
    class ImagePreviewDialog;
}

class QnImagePreviewDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit QnImagePreviewDialog(QWidget *parent = 0);
    ~QnImagePreviewDialog();
    
    void openImage(const QString &filename);

private slots:
    void setImage(const QImage& image);

private:
    QScopedPointer<Ui::ImagePreviewDialog> ui;
};

#endif // IMAGE_PREVIEW_DIALOG_H
