// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "progress_widget.h"

#include <QtWidgets/QBoxLayout>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/loading_indicator.h>

QnProgressWidget::QnProgressWidget(QWidget *parent):
    QWidget(parent),
    m_img(new QLabel(this)),
    m_text(new QLabel(this))
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);

    using LoadingIndicator = nx::vms::client::desktop::LoadingIndicator;
    auto indicator = new LoadingIndicator(this);
    connect(indicator, &LoadingIndicator::frameChanged, this,
        [this](const QPixmap& pixmap) { m_img->setPixmap(pixmap); });

    m_img->setFixedSize(nx::vms::client::core::kIconSize);
    m_img->setScaledContents(true);

    layout->addWidget(m_img);
    layout->addWidget(m_text);
    m_text->setVisible(false);
    setLayout(layout);
}

void QnProgressWidget::setText(const QString &text) {
    m_text->setText(text);
    m_text->setVisible(!text.isEmpty());
}
