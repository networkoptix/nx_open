#ifndef settings_widgets_h_1820
#define settings_widgets_h_1820

#include <QtGui/QSlider>

#include <core/resource/param.h>

class QCheckBox;
class QGroupBox;
class QRadioButton;

class SettingsSlider : public QSlider
{
    Q_OBJECT

public:
    explicit SettingsSlider(Qt::Orientation orientation, QWidget *parent = 0);

Q_SIGNALS:
    void onKeyReleased();

protected:
    void keyReleaseEvent(QKeyEvent *event);
};

//==============================================

class CLAbstractSettingsWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QVariant paramValue READ paramValue WRITE setParamValue NOTIFY paramValueChanged USER true DESIGNABLE false)

public:
    CLAbstractSettingsWidget(const QnParam &param, QWidget *parent = 0);
    virtual ~CLAbstractSettingsWidget();

    const QnParam &param() const;

    QVariant paramValue() const;
    void setParamValue(const QVariant &value);

Q_SIGNALS:
    void paramValueChanged(const QVariant &value);

protected:
    virtual void updateParam(const QVariant &value) = 0;

private:
    Q_DISABLE_COPY(CLAbstractSettingsWidget)

    QnParam m_param;
};
//==============================================

class SettingsOnOffWidget : public CLAbstractSettingsWidget
{
    Q_OBJECT

public:
    SettingsOnOffWidget(const QnParam &param, QWidget *parent = 0);

protected:
    void updateParam(const QVariant &value);

private Q_SLOTS:
    void toggled(bool checked);

private:
    QCheckBox *m_checkBox;
};
//==============================================

class SettingsMinMaxStepWidget : public CLAbstractSettingsWidget
{
    Q_OBJECT

public:
    SettingsMinMaxStepWidget(const QnParam &param, QWidget *parent = 0);

protected:
    void updateParam(const QVariant &value);

private Q_SLOTS:
    void onValueChanged();
    void onValueChanged(int value);

private:
    SettingsSlider *m_slider;
    QGroupBox *m_groupBox;
};
//==============================================

class SettingsEnumerationWidget : public CLAbstractSettingsWidget
{
    Q_OBJECT

public:
    SettingsEnumerationWidget(const QnParam &param, QWidget *parent = 0);

protected:
    void updateParam(const QVariant &value);

private Q_SLOTS:
    void onClicked();

private:
    QList<QRadioButton *> m_radioButtons;
};

//==============================================
class SettingsButtonWidget : public CLAbstractSettingsWidget
{
    Q_OBJECT

public:
    SettingsButtonWidget(const QnParam &param, QWidget *parent = 0);

protected:
    void updateParam(const QVariant &value);

private Q_SLOTS:
    void onClicked();
};


//==============================================
#include <QtGui/QItemEditorFactory>

class SettingsEditorFactory : public QItemEditorFactory
{
public:
    SettingsEditorFactory();

    QWidget *createEditor(const QnParam &param, QWidget *parent) const;
    QByteArray valuePropertyName(const QnParam &param) const;
};

#endif //settings_widgets_h_1820
