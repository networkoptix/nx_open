#ifndef SIGNDIALOG_H
#define SIGNDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QVBoxLayout>

#include "plugins/resources/archive/avi_files/avi_resource.h"

class QDataWidgetMapper;
class QStandardItemModel;
class QUrl;

class QnWorkbenchContext;
class QnAbstractArchiveReader;
class QnResourceWidgetRenderer;
class QnSignDialogGlWidget;
class QnCamDisplay;
class QnSignInfo;

namespace Ui {
    class SignDialog;
}

class SignDialog : public QDialog {
    Q_OBJECT;

public:
    explicit SignDialog(QnResourcePtr resource, QWidget *parent = 0);
    virtual ~SignDialog();

    static QRect calcVideoRect(double windowWidth, double windowHeight, double textureWidth, double textureHeight);

public slots:
    virtual void accept() override;

private slots:
    void at_calcSignInProgress(QByteArray sign, int progress);
    void at_gotImageSize(int width, int height);
    void at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame);
protected:
    virtual void changeEvent(QEvent *event) override;

private:
    Q_DISABLE_COPY(SignDialog)

    QScopedPointer<Ui::SignDialog> ui;
    
    QnAviResourcePtr m_resource;

    QnCamDisplay *m_camDispay;
    QnAbstractArchiveReader *m_reader;
    QnResourceWidgetRenderer *m_renderer;
    QnSignDialogGlWidget *m_glWindow;
    QnSignInfo* m_srcVideoInfo;
    QVBoxLayout* m_layout;

    int m_requestHandle;
};

#endif // SIGNDIALOG_H
