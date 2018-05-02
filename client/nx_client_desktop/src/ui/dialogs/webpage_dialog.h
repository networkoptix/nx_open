#pragma once

#include <ui/dialogs/common/button_box_dialog.h>

namespace Ui {
class WebpageDialog;
}

class QnWebpageDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit QnWebpageDialog(QWidget* parent);
    virtual ~QnWebpageDialog();

    QString name() const;
    void setName(const QString& name);

    QString url() const;
    void setUrl(const QString& url);

    virtual void accept() override;

private:
    QScopedPointer<Ui::WebpageDialog> ui;
};
