#include "select_resources_delegate_editor_button.h"

QnSelectResourcesDialogButton::QnSelectResourcesDialogButton(QWidget* parent):
    base_type(parent)
{
    connect(this, &QnSelectResourcesDialogButton::clicked,
        this, &QnSelectResourcesDialogButton::handleButtonClicked);
}

QnSelectResourcesDialogButton::~QnSelectResourcesDialogButton()
{
}

QnUuidSet QnSelectResourcesDialogButton::getResources() const
{
    return m_resources;
}

void QnSelectResourcesDialogButton::setResources(const QnUuidSet& resources)
{
    m_resources = resources;
}
