// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "empty_business_event_widget.h"
#include "ui_empty_business_event_widget.h"

QnEmptyBusinessEventWidget::QnEmptyBusinessEventWidget(QWidget *parent) :
    base_type(parent),
    ui(new Ui::EmptyBusinessEventWidget)
{
    ui->setupUi(this);
}

QnEmptyBusinessEventWidget::~QnEmptyBusinessEventWidget()
{
}

