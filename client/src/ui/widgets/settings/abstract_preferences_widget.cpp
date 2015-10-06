#include "abstract_preferences_widget.h"

QnAbstractPreferencesWidget::QnAbstractPreferencesWidget(QWidget *parent /*= 0*/) 
    : QWidget(parent)
    , m_readOnly(false)
{}

void QnAbstractPreferencesWidget::submitToSettings() {
}


void QnAbstractPreferencesWidget::updateFromSettings() {
}

bool QnAbstractPreferencesWidget::confirm() {
    return true;
}

bool QnAbstractPreferencesWidget::discard() {
    return true;
}

bool QnAbstractPreferencesWidget::hasChanges() const {
    return false;
}

void QnAbstractPreferencesWidget::retranslateUi() {
}

bool QnAbstractPreferencesWidget::isReadOnly() const {
    return m_readOnly;
}

void QnAbstractPreferencesWidget::setReadOnly(bool readOnly) {
    if (m_readOnly == readOnly)
        return;
    setReadOnlyInternal(readOnly);
    m_readOnly = readOnly;
}

void QnAbstractPreferencesWidget::setReadOnlyInternal(bool readOnly) {
}


