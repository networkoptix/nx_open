#pragma once

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
class QnSignDialogDisplay;
class QnSignInfo;

class QnAviResource;

namespace Ui {
class SignDialog;
}

class SignDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit SignDialog(QnResourcePtr resource, QWidget *parent);
    virtual ~SignDialog();

    static QRect calcVideoRect(double windowWidth, double windowHeight, double textureWidth, double textureHeight);

private:
    void at_calcSignInProgress(QByteArray sign, int progress);
    void at_gotImageSize(int width, int height);
    void at_gotSignature(QByteArray calculatedSign, QByteArray signFromFrame);

protected:
    virtual void changeEvent(QEvent *event) override;

private:
    Q_DISABLE_COPY(SignDialog)

    QScopedPointer<Ui::SignDialog> ui;

    QnSharedResourcePointer<QnAviResource> m_resource;

    QScopedPointer<QnSignDialogDisplay> m_camDispay;
    QScopedPointer<QnAbstractArchiveStreamReader> m_reader;
    QScopedPointer<QnSignDialogGlWidget> m_glWindow;
    QnResourceWidgetRenderer *m_renderer = nullptr;
    QnSignInfo* m_srcVideoInfo = nullptr;
    QVBoxLayout* m_layout = nullptr;
    int m_requestHandle = -1;
};
