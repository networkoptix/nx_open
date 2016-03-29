#pragma once

#include <QtWidgets/QPushButton>

#include <ui/workbench/workbench_context_aware.h>

class QnCloudStatusPanelPrivate;

class QnCloudStatusPanel : public QPushButton, QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QPushButton base_type;

public:
    explicit QnCloudStatusPanel(QWidget *parent = nullptr);
    ~QnCloudStatusPanel();

private:
    QScopedPointer<QnCloudStatusPanelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCloudStatusPanel)
};
