#ifndef IMAGE_PREVIEW_DIALOG_H
#define IMAGE_PREVIEW_DIALOG_H

#include <QtWidgets/QDialog>

#include <ui/dialogs/common/dialog.h>

namespace Ui {
    class ImagePreviewDialog;
}

class QnImagePreviewDialog : public QnDialog
{
    Q_OBJECT
    
    typedef QnDialog base_type;

public:
    explicit QnImagePreviewDialog(QWidget *parent);
    ~QnImagePreviewDialog();
    
    void openImage(const QString &filename);

private slots:
    void setImage(const QImage& image);

private:
    QScopedPointer<Ui::ImagePreviewDialog> ui;
};

#endif // IMAGE_PREVIEW_DIALOG_H
