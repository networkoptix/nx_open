#include "select_resources_delegate_editor_button.h"

QnSelectResourcesDialogButton::QnSelectResourcesDialogButton(QWidget* parent):
    base_type(parent)
{
}

QnSelectResourcesDialogButton::~QnSelectResourcesDialogButton()
{
}

QnUuidSet QnSelectResourcesDialogButton::getResources() const
{
    return m_resources;
}

void QnSelectResourcesDialogButton::setResources(QnUuidSet resources)
{
    m_resources = resources;
}
