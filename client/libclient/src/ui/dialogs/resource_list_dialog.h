#pragma once

#include <QtWidgets/QDialog>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

class QnResourceListModel;

namespace Ui {
    class ResourceListDialog;
}

class QnResourceListDialog: public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT;
    using base_type = QnWorkbenchStateDependentButtonBoxDialog;

public:
    QnResourceListDialog(QWidget* parent = nullptr);
    virtual ~QnResourceListDialog();

    void setText(const QString& text);
    QString text() const;

    void setBottomText(const QString& bottomText);
    QString bottomText() const;

    void setStandardButtons(QDialogButtonBox::StandardButtons standardButtons);
    QDialogButtonBox::StandardButtons standardButtons() const;

    const QnResourceList& resources() const;
    void setResources(const QnResourceList& resources);

    void showIgnoreCheckbox();
    bool isIgnoreCheckboxChecked() const;

    using QnButtonBoxDialog::exec;

    static QDialogButtonBox::StandardButton exec(QWidget* parent, const QnResourceList& resources,
        const QString& title, const QString& text, const QString& bottomText,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        bool readOnly = true);

    static QDialogButtonBox::StandardButton exec(QWidget* parent, const QnResourceList& resources,
        const QString& title, const QString& text,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        bool readOnly = true);

    static QDialogButtonBox::StandardButton exec(QWidget* parent, const QnResourceList& resources,
        int helpTopicId, const QString& title, const QString& text, const QString& bottomText,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        bool readOnly = true);

    static QDialogButtonBox::StandardButton exec(QWidget* parent, const QnResourceList& resources,
        int helpTopicId, const QString& title, const QString& text,
        QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        bool readOnly = true);

protected:
    virtual void setReadOnlyInternal() override;

private:
    QScopedPointer<Ui::ResourceListDialog> ui;
    QnResourceListModel* m_model;
};
