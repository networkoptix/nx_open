#ifndef SIGNDIALOG_H
#define SIGNDIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/session_aware_dialog.h>

class QDataWidgetMapper;
class QStandardItemModel;
class QUrl;

class QnWorkbenchContext;
class QnAbstractArchiveStreamReader;
class QnResourceWidgetRenderer;
class QnSignDialogGlWidget;
class QnCamDisplay;
class QnSignInfo;

class QnAviResource;

namespace Ui {
    class SignDialog;
}

class SignDialog : public QnSessionAwareButtonBoxDialog {
    Q_OBJECT;

    typedef QnSessionAwareButtonBoxDialog base_type;
public:
    explicit SignDialog(QnResourcePtr resource, QWidget *parent = 0);
    virtual ~SignDialog();

    static QRect calcVideoRect(double windowWidth, double windowHeight, double textureWidth, double textureHeight);

private slots:
    void at_calcSignInProgress(QByteArray sign, int progress);
    void at_gotImageSize(int width, int height);
    void at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame);
protected:
    virtual void changeEvent(QEvent *event) override;

private:
    Q_DISABLE_COPY(SignDialog)

    QScopedPointer<Ui::SignDialog> ui;
    
    QnSharedResourcePointer<QnAviResource> m_resource;

    QnCamDisplay *m_camDispay;
    QnAbstractArchiveStreamReader *m_reader;
    QnResourceWidgetRenderer *m_renderer;
    QnSignDialogGlWidget *m_glWindow;
    QnSignInfo* m_srcVideoInfo;
    QVBoxLayout* m_layout;

    int m_requestHandle;
};

#endif // SIGNDIALOG_H
