#include "camera_advanced_settings_widget.h"

#include <QtGui/QApplication>
#include <QtGui/QStyle>
#include <QtGui/QStyleFactory>

//#include "device/device.h"
//#include "base/log.h"
//#include "settings.h"

QnSettingsSlider::QnSettingsSlider(Qt::Orientation orientation, QWidget *parent)
: QSlider(orientation, parent)
{
    //setStyle(QStyleFactory::create(qApp->style()->objectName()));
}

void QnSettingsSlider::keyReleaseEvent(QKeyEvent *event)
{
    QSlider::keyReleaseEvent(event);

    emit onKeyReleased();
}
//==============================================

QnAbstractSettingsWidget::QnAbstractSettingsWidget(QObject* handler, CameraSetting& obj)
    //: QObject(),
    : QWidget(),
      mHandler(handler),
      mParam(obj),
      mWidget(0)
{
    //QObject::connect(this, SIGNAL( setParam(const QString&, const CameraSettingValue&) ),
    //    handler, SLOT( setParam(const QString&, const CameraSettingValue&) )  );
    connect(this, SIGNAL( setParam(const QString&, const CameraSettingValue&) ),
        handler, SLOT( setParam(const QString&, const CameraSettingValue&) )  );
}

QnAbstractSettingsWidget::~QnAbstractSettingsWidget()
{

}

const CameraSetting& QnAbstractSettingsWidget::param() const
{
    return mParam;
}

QWidget* QnAbstractSettingsWidget::toWidget()
{
    return mWidget;
}

void QnAbstractSettingsWidget::setParam_helper(const QString& /*name*/, const CameraSettingValue& val)
{
    mParam.setCurrent(val);
    emit setParam(mParam.getName(), val);
}

//==============================================
QnSettingsOnOffWidget::QnSettingsOnOffWidget(QObject* handler, CameraSetting& obj, QWidget& parent):
    QnAbstractSettingsWidget(handler, obj)
{
    m_checkBox = new QCheckBox(mParam.getName(), &parent);
    if (mParam.getCurrent() == mParam.getMax())
        m_checkBox->setCheckState(Qt::Checked);

    QObject::connect(m_checkBox, SIGNAL(stateChanged ( int )), this, SLOT(stateChanged(int)));

    //QPalette plt;	plt.setColor(QPalette::WindowText, Qt::white);	checkBox->setPalette(plt);//black

    mWidget = m_checkBox;
}

QnSettingsOnOffWidget::~QnSettingsOnOffWidget()
{
}

void QnSettingsOnOffWidget::stateChanged(int state)
{
    //if (mParam.value.possible_values.count()<2)
    //{
    //    cl_log.log(QLatin1String("param.value.possible_values.count()<2 !!!!"), cl_logERROR);
    //    return;
    //}

    QString val = state == Qt::Checked ? (QString)mParam.getMax() : (QString)mParam.getMin();
    setParam_helper(mParam.getName(), val);
}

void QnSettingsOnOffWidget::updateParam(QString val)
{
    m_checkBox->setChecked(val == mParam.getMax()); // emits stateChanged
}

//==============================================
QnSettingsMinMaxStepWidget::QnSettingsMinMaxStepWidget(QObject* handler, CameraSetting& obj, QWidget& parent):
    QnAbstractSettingsWidget(handler, obj)
{
    groupBox = new QGroupBox(&parent);

    m_slider = new QnSettingsSlider(Qt::Horizontal, groupBox);
    //m_slider->setMinimumWidth(130);
    //m_slider->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    groupBox->setTitle(mParam.getName());

    ////QFont font("Courier New", 11);
    ////groupBox->setFont(font);

    m_slider->setRange(mParam.getMin(), mParam.getMax());
    m_slider->setValue(mParam.getCurrent());

    groupBox->setTitle(mParam.getName() + QLatin1Char('(') + (QString)mParam.getCurrent() + QLatin1Char(')'));

    QVBoxLayout *layout = new QVBoxLayout(groupBox);
    layout->addWidget(m_slider);

    QObject::connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onValChanged()));
    QObject::connect(m_slider, SIGNAL(onKeyReleased()), this, SLOT(onValChanged()));
    QObject::connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onValChanged(int)) );

    mWidget = groupBox;
}

void QnSettingsMinMaxStepWidget::onValChanged(int val)
{
    groupBox->setTitle(mParam.getName() + QLatin1Char('(') + QString::number(val) + QLatin1Char(')'));
}

void QnSettingsMinMaxStepWidget::onValChanged()
{
    setParam_helper(mParam.getName(), m_slider->value());
}

void QnSettingsMinMaxStepWidget::updateParam(QString val)
{
    CameraSettingValue cv(val);
    m_slider->setValue(cv);
}

//==============================================
QnSettingsEnumerationWidget::QnSettingsEnumerationWidget(QObject* handler, CameraSetting& obj, QWidget& parent):
    QnAbstractSettingsWidget(handler, obj)
{
    QGroupBox* groupBox  = new QGroupBox(&parent);
    mWidget = groupBox;
    //groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    groupBox->setTitle(mParam.getName());
    //groupBox->setMinimumWidth(140);

    QVBoxLayout *layout = new QVBoxLayout(groupBox);

    QStringList values = static_cast<QString>(mParam.getMin()).split(QLatin1Char(','));
    mParam.setCurrent(values[0].trimmed());//ToDo remove this line
    for (int i = 0; i < values.length(); ++i)
    {
        QString val = values[i].trimmed();

        QRadioButton *btn = new QRadioButton(val, groupBox);
        layout->addWidget(btn);

        if (val == mParam.getCurrent())
            btn->setChecked(true);

        btn->setObjectName(val);
        //btn->setFont(settings_font);
    
        m_radioBtns.push_back(btn);

        connect(btn , SIGNAL(clicked()), this, SLOT(onClicked()));
    }
}

void QnSettingsEnumerationWidget::onClicked()
{
    QString val = QObject::sender()->objectName();

    setParam_helper(mParam.getName(), val);
}

void QnSettingsEnumerationWidget::updateParam(QString val)
{
    QRadioButton* btn = getBtnByname(val);
    if (btn)
        btn->setChecked(true);
}

QRadioButton* QnSettingsEnumerationWidget::getBtnByname(const QString& name)
{
    foreach(QRadioButton* btn, m_radioBtns)
    {
        if (btn->objectName() == name)
            return btn;
    }

    return 0;
}

//==================================================
QnSettingsButtonWidget::QnSettingsButtonWidget(QObject* handler, CameraSetting& obj, QWidget& parent):
    QnAbstractSettingsWidget(handler, obj)
{
    QPushButton* btn = new QPushButton(mParam.getName(), &parent);

    QObject::connect(btn, SIGNAL(released()), this, SLOT(onClicked()));

    btn->setFocusPolicy(Qt::NoFocus);

    mWidget = btn;
}

void QnSettingsButtonWidget::onClicked()
{
    setParam_helper(mParam.getName(), "");
}

void QnSettingsButtonWidget::updateParam(QString /*val*/)
{
    //cl_log.log("updateParam", cl_logALWAYS);
}
