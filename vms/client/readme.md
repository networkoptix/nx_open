# UI Widgets Library {#widgets_lib}
General style specification: https://4yppjt.axshare.com/#p=colors&g=1

## Examples

### Checkbox, displayed as a button
![](images/checkbox.png)
**Source File / Class:** *QCheckBox*

**Parameters, Usage example**:

checkBox->setProperty(style::Properties::kCheckBoxAsButton, true);
checkBox->setForegroundRole(QPalette::ButtonText);

### Panel with lighter color
![](images/panel.png)
**Source File / Class:** *Panel: QWidget*

nx/client/desktop/common/widgets/panel.h

### Thin dark line
![](images/panel.png)
**Source File / Class:** *QLine*

**Parameters, Usage example**:

line->setFrameShadow(QFrame::Plain);
line->setMaximumHeight(1);

### Info banner
![](images/banner1.png)
![](images/banner2.png)

**Source File / Class:** *InfoBanner: QLabel*

nx/client/desktop/common/widgets/info_banner.h

**Parameters, Usage example**:

banner->setWarningStyle(true);