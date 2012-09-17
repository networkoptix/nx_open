#ifndef abstract_device_settings_dlg_h_1652
#define abstract_device_settings_dlg_h_1652

#include <QtCore/QHash>
#include <QtCore/QList>

#include <QtGui/QDialog>

#include "core/resource/resource.h"

class QDialogButtonBox;
class QGroupBox;
class QTabWidget;

class CLDeviceSettingsTab;

class CLAbstractDeviceSettingsDlg : public QDialog
{
    Q_OBJECT

public:
    CLAbstractDeviceSettingsDlg(QnResourcePtr resource, QWidget *parent = 0);
    virtual ~CLAbstractDeviceSettingsDlg();

    QnResourcePtr resource() const;

    QList<CLDeviceSettingsTab *> tabs() const;
    CLDeviceSettingsTab *tabByName(const QString &name) const;
    void addTab(CLDeviceSettingsTab *tab, const QString &name);

    QList<QGroupBox *> groups() const;
    QGroupBox *groupByName(const QString &name) const;
    void putGroup(const QString &name, QGroupBox *group);

    QList<QWidget *> widgets() const;
    QList<QWidget *> widgetsByGroup(const QString &group, const QString &subGroup = QString()) const;
    QWidget *widgetByName(const QString &name) const;
    void registerWidget(QWidget *widget, const QnParam &param);

    QnParam param(QWidget *widget) const;

public Q_SLOTS:
    virtual void accept();
    virtual void reject();
    virtual void reset();

protected Q_SLOTS:
    virtual void buildTabs();

protected:
    void buildTabs(QTabWidget *tabWidget, const QList<QString> &tabsOrder);
    inline void buildTabs(const QList<QString> &tabsOrder)
    { buildTabs(m_tabWidget, tabsOrder); }

    void setParam(const QString &paramName, const QVariant &value);

private Q_SLOTS:
    void saveParam();

    void saveSuccess();
    void saveError();

    void currentTabChanged(int index);

protected:
    QTabWidget *m_tabWidget;
    QDialogButtonBox *m_buttonBox;

private:
    const QnResourcePtr m_resource;
    const QnParamList m_params; // a snaphost of resource's param list

    QHash<QString, QWidget *> m_widgets;
    QHash<QString, QGroupBox *> m_groups;
    QHash<QString, CLDeviceSettingsTab *> m_tabs;
};

#endif //abstract_device_settings_dlg_h_1652
