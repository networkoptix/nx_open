#ifndef global_settings_h1933
#define global_settings_h1933

extern qreal global_rotation_angel;
extern bool global_show_item_text;
extern QFont settings_font;
extern int devices_update_interval;
extern QColor global_shadow_color;
extern QColor app_bkr_color;
extern QColor global_selection_color;
extern QColor global_can_be_droped_color;

extern qreal global_menu_opacity;
extern qreal global_dlg_opacity;
extern qreal global_decoration_opacity;
extern qreal global_decoration_max_opacity;

extern qreal global_base_scene_z_level;

extern int global_grid_aparence_delay;
extern int global_opacity_change_period;

class Settings
{
public:
    static Settings& instance();

    struct Data
    {
        bool allowChangeIP;
        QString mediaRoot;
        QStringList auxMediaRoots;
    };

    void update(const Data& data);
    void fillData(Data& data) const;

    void load(const QString& fileName);
    void save();

    bool isAllowChangeIP() const;
    QString mediaRoot() const;
    QStringList auxMediaRoots() const;

    bool haveValidSerialNumber() const;
    void setSerialNumber(const QString& serial);

    void addAuxMediaRoot(const QString& root);

private:
    Settings();
    Settings(const Settings&) {}

    void setAllowChangeIP(bool allow);
    void setMediaRoot(const QString& root);
    void setAuxMediaRoots(const QStringList&);
    void removeAuxMediaRoot(const QString& root);
    void reset();

private:
    mutable QReadWriteLock m_RWLock;
    QString m_fileName;

    Data m_data;

    QString m_serialNumber;
    bool m_haveValidSerialNumber;
};

#endif //global_settings_h1933
