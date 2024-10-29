// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "word_wrapped_label.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QResizeEvent>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>

QnWordWrappedLabel::QnWordWrappedLabel(QWidget* parent):
    QWidget(parent),
    m_label(new QLabel(this)),
    m_rowHeight(QFontMetrics(m_label->font()).height())
{
    m_label->setWordWrap(true);
    nx::vms::client::desktop::anchorWidgetToParent(m_label);

    connect(m_label, &QLabel::linkActivated, this, &QnWordWrappedLabel::linkActivated);
    connect(this, &QObject::objectNameChanged,
        [this](const QString& name)
        {
            m_label->setObjectName(name);
        });
}

QLabel* QnWordWrappedLabel::label() const
{
    return m_label;
}

void QnWordWrappedLabel::showEvent(QShowEvent* event)
{
    ensureInitialized();
    base_type::showEvent(event);
}

void QnWordWrappedLabel::ensureInitialized()
{
    if (m_initialized)
        return;

    m_initialized = true;
    updateGeometry();
}

QSize QnWordWrappedLabel::sizeHint() const
{
    if (m_initialized && text().isEmpty())
        return {};

    QSize result(m_label->sizeHint().width(),
        m_label->heightForWidth(this->geometry().width()));

    if (m_initialized && result.isValid())
        return result;

    return QSize(m_rowHeight, m_rowHeight);
}

QSize QnWordWrappedLabel::minimumSizeHint() const
{
    return sizeHint();
}

bool QnWordWrappedLabel::hasHeightForWidth() const
{
    return true;
}

int QnWordWrappedLabel::heightForWidth(int width) const
{
    return m_label->heightForWidth(width);
}

QString QnWordWrappedLabel::text() const
{
    return m_label->text();
}

void QnWordWrappedLabel::setText(const QString& value)
{
    if (text() == value)
        return;

    m_label->setText(value);
    updateGeometry();
}

Qt::TextFormat QnWordWrappedLabel::textFormat() const
{
    return m_label->textFormat();
}

void QnWordWrappedLabel::setTextFormat(Qt::TextFormat value)
{
    if (textFormat() == value)
        return;

    m_label->setTextFormat(value);
    updateGeometry();
}

bool QnWordWrappedLabel::openExternalLinks() const
{
    return m_label->openExternalLinks();
}

void QnWordWrappedLabel::setOpenExternalLinks(bool value)
{
    m_label->setOpenExternalLinks(value);
}

QPalette::ColorRole QnWordWrappedLabel::foregroundRole() const
{
    return m_label->foregroundRole();
}

void QnWordWrappedLabel::setForegroundRole(QPalette::ColorRole role)
{
    m_label->setForegroundRole(role);
}
