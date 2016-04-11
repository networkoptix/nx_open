#pragma once

#include <QtWidgets/QToolButton>

#include <ui/workbench/workbench_context_aware.h>

class QnCloudStatusPanelPrivate;

class QnCloudStatusPanel : public QToolButton, QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QToolButton base_type;

public:
    explicit QnCloudStatusPanel(QWidget *parent = nullptr);
    ~QnCloudStatusPanel();

private:
    QScopedPointer<QnCloudStatusPanelPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnCloudStatusPanel)
};
