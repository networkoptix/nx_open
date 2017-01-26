#include "abstract_preferences_widget.h"

QnAbstractPreferencesWidget::QnAbstractPreferencesWidget(QWidget *parent /*= 0*/)
    : QWidget(parent)
    , m_readOnly(false)
{}

void QnAbstractPreferencesWidget::discardChanges()
{
    /* Do nothing by default. */
}

bool QnAbstractPreferencesWidget::canApplyChanges() const
{
    return true;
}

bool QnAbstractPreferencesWidget::canDiscardChanges() const
{
    return true;
}

void QnAbstractPreferencesWidget::retranslateUi()
{
}

bool QnAbstractPreferencesWidget::isReadOnly() const
{
    return m_readOnly;
}

void QnAbstractPreferencesWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;
    setReadOnlyInternal(readOnly);
    m_readOnly = readOnly;
}

void QnAbstractPreferencesWidget::setReadOnlyInternal(bool readOnly)
{
    Q_UNUSED(readOnly);
}


