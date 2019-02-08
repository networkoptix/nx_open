#pragma once

#include <QtWidgets/QLineEdit>
#include <QtWidgets/QLabel>

typedef std::function<bool(const QString&)> ValidateFunction;

bool defaultValidateFunction(const QString &text);

/**
 * @brief declareMandatoryField         Mark the field as mandatory, so field label will turn red (warning style) if the field value is invalid. 
 * @param label                         Label whose color will be changed. 
 * @param lineEdit                      Text field that will be checked. If not set, label's buddy will be used.
 * @param isValid                       Text validate function, must return True if the text value is valid. 
 *                                      By default, any non-empty (after trim) string is counted as valid.
 */ 
void declareMandatoryField(QLabel* label, QLineEdit* lineEdit = nullptr, ValidateFunction isValid = defaultValidateFunction);
