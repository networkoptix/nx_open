// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

UuidSet QnSelectResourcesDialogButton::getResources() const
{
    return m_resources;
}

void QnSelectResourcesDialogButton::setResources(const UuidSet& resources)
{
    m_resources = resources;
}
