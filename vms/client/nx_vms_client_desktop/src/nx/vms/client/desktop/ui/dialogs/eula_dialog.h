#pragma once

#include <ui/dialogs/common/dialog.h>

#include <nx/utils/impl_ptr.h>

namespace Ui { class EulaDialog; }

namespace nx::vms::client::desktop {

class WebEngineView;

class EulaDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    explicit EulaDialog(QWidget* parent = nullptr);
    virtual ~EulaDialog() override;

    QString title() const;
    void setTitle(const QString& value);

    void setEulaHtml(const QString& value);

    virtual bool hasHeightForWidth() const override;

    static bool acceptEulaHtml(const QString& html, int version, QWidget* parent = nullptr);
    static bool acceptEulaFromFile(const QString& path, int version, QWidget* parent = nullptr);

protected:
    virtual bool event(QEvent* event) override;

private:
    void updateSize();

private:
    nx::utils::ImplPtr<Ui::EulaDialog> ui;
    WebEngineView* m_webEngineView;
};

} // namespace nx::vms::client::desktop
