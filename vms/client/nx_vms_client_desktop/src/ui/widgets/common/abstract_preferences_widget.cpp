#include "abstract_preferences_widget.h"

QnAbstractPreferencesWidget::QnAbstractPreferencesWidget(QWidget* parent):
    base_type(parent)
{
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
