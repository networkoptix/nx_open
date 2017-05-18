#pragma once

class QCheckBox;

struct QnCheckboxUtils
{
    static void autoClearTristate(QCheckBox* checkbox);
    static void setupTristateCheckbox(QCheckBox* checkbox, bool sameValue, bool checked);
};
