#pragma once

#include <ui/dialogs/common/button_box_dialog.h>

namespace Ui {
class WebpageDialog;
}

namespace nx { namespace vms { namespace api { enum class WebPageSubtype; }}}

class QnWebpageDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit QnWebpageDialog(QWidget* parent);
    virtual ~QnWebpageDialog();

    QString name() const;
    void setName(const QString& name);

    QUrl url() const;
    void setUrl(const QUrl& url);

    nx::vms::api::WebPageSubtype subtype() const;
    void setSubtype(nx::vms::api::WebPageSubtype value);

    virtual void accept() override;

private:
    QScopedPointer<Ui::WebpageDialog> ui;
};
