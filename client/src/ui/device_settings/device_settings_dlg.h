#ifndef abstract_device_settings_dlg_h_1652
#define abstract_device_settings_dlg_h_1652

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
    CLAbstractDeviceSettingsDlg(QnResourcePtr resource);
    virtual ~CLAbstractDeviceSettingsDlg();

    QnResourcePtr resource() const;

    void putWidget(CLAbstractSettingsWidget *wgt);
    void putGroup(QGroupBox *group);

public Q_SLOTS:
    virtual void setParam(const QString &name, const QVariant &val);
    virtual void onClose();
    virtual void onSuggestions();

    virtual void onNewtab(int index);

protected:
    void addTab(CLDeviceSettingsTab *tab);
    CLAbstractSettingsWidget *getWidgetByName(const QString &name) const;
    QGroupBox *getGroupByName(const QString &name) const;
    CLDeviceSettingsTab *tabByName(const QString &name) const;

    QList<CLAbstractSettingsWidget *> getWidgetsBygroup(const QString &group) const;

protected:
    QnResourcePtr m_resource;

    QTabWidget *m_tabWidget;
    QDialogButtonBox *m_buttonBox;

    QList<CLAbstractSettingsWidget *> mWgtsLst;
    QList<QGroupBox *> m_groups;
    QList<CLDeviceSettingsTab *> m_tabs;
};

#endif //abstract_device_settings_dlg_h_1652
