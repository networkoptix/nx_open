#ifndef abstract_device_settings_dlg_h_1652
#define abstract_device_settings_dlg_h_1652

#include <QtCore/QHash>
#include <QtCore/QList>

#include <QtGui/QDialog>

#include "core/resource/resource.h"

class QDialogButtonBox;
class QGroupBox;
class QTabWidget;

class CLAbstractSettingsWidget;
class CLDeviceSettingsTab;

class CLAbstractDeviceSettingsDlg : public QDialog
{
    Q_OBJECT

public:
    CLAbstractDeviceSettingsDlg(QnResourcePtr resource, QWidget *parent = 0);
    virtual ~CLAbstractDeviceSettingsDlg();

    QnResourcePtr resource() const;

    CLDeviceSettingsTab *tabByName(const QString &name) const;
    void addTab(CLDeviceSettingsTab *tab);

    QGroupBox *groupByName(const QString &name) const;
    void putGroup(const QString &name, QGroupBox *group);

    QList<CLAbstractSettingsWidget *> widgetsByGroup(const QString &group) const;
    CLAbstractSettingsWidget *widgetByName(const QString &name) const;
    void putWidget(CLAbstractSettingsWidget *wgt);

public Q_SLOTS:
    virtual void setParam(const QString &name, const QVariant &val);
    virtual void onClose();
    virtual void onSuggestions();

    virtual void onNewtab(int index);

protected:
    QnResourcePtr m_resource;

    QTabWidget *m_tabWidget;
    QDialogButtonBox *m_buttonBox;

    QHash<QString, CLAbstractSettingsWidget *> m_widgets;
    QHash<QString, QGroupBox *> m_groups;
    QList<CLDeviceSettingsTab *> m_tabs;
};

#endif //abstract_device_settings_dlg_h_1652
