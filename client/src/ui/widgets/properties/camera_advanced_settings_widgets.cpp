#include "camera_advanced_settings_widgets.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QVBoxLayout>


QnSettingsScrollArea::QnSettingsScrollArea(QWidget* parent):
    QScrollArea(parent)
{
    QWidget *widget = new QWidget();
    widget->setLayout(new QVBoxLayout());
    setWidget(widget);
    if(isVisible())
        widget->show();

    setWidgetResizable(true);
}

void QnSettingsScrollArea::addWidget(QWidget *widgetToAdd)
{
    if (widget()->layout()->count() != 0)
        widget()->layout()->removeItem(widget()->layout()->itemAt(widget()->layout()->count() - 1));

    widgetToAdd->setParent(widget());
    dynamic_cast<QVBoxLayout*>(widget()->layout())->addWidget(widgetToAdd);
    static_cast<QVBoxLayout*>(widget()->layout())->addStretch(1);
}

//==============================================

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

QnAbstractSettingsWidget::QnAbstractSettingsWidget(const CameraSetting &obj, QnSettingsScrollArea *parent, const QString& hint)
    : QWidget(0),
      m_param(obj),
      m_layout(new QHBoxLayout()),
      m_hint(hint)
{
    if (parent)
        parent->addWidget(this);
    setLayout(m_layout);
    setToolTip(m_hint);
}

QnAbstractSettingsWidget::~QnAbstractSettingsWidget()
{

}

CameraSetting QnAbstractSettingsWidget::param() const
{
    return m_param;
}

void QnAbstractSettingsWidget::setParam(const CameraSettingValue& val)
{
    m_param.setCurrent(val);
    emit advancedParamChanged(m_param);
}

//==============================================
QnSettingsOnOffWidget::QnSettingsOnOffWidget(const CameraSetting &obj, QnSettingsScrollArea *parent):
    QnAbstractSettingsWidget(obj, parent, obj.getDescription())
{
    m_checkBox = new QCheckBox(m_param.getName());
    m_layout->addWidget(m_checkBox);

    if (m_param.getCurrent() == m_param.getMax())
        m_checkBox->setCheckState(Qt::Checked);

    connect(m_checkBox, SIGNAL(stateChanged ( int )), this, SLOT(stateChanged(int)));
}

void QnSettingsOnOffWidget::refresh()
{
    if (m_param.getCurrent() == m_param.getMax()) {
        m_checkBox->setCheckState(Qt::Checked);
    } else {
        m_checkBox->setCheckState(Qt::Unchecked);
    }

}

QnSettingsOnOffWidget::~QnSettingsOnOffWidget()
{
}

void QnSettingsOnOffWidget::stateChanged(int state)
{
    QString val = state == Qt::Checked ? (QString)m_param.getMax() : (QString)m_param.getMin();
    setParam(val);
}

void QnSettingsOnOffWidget::updateParam(QString val)
{
    m_checkBox->setChecked(val == m_param.getMax()); // emits stateChanged
}

//==============================================
QnSettingsMinMaxStepWidget::QnSettingsMinMaxStepWidget(const CameraSetting &obj, QnSettingsScrollArea *parent):
    QnAbstractSettingsWidget(obj, parent, obj.getDescription())
{
    QVBoxLayout *vlayout = new QVBoxLayout();
    m_layout->addLayout(vlayout);

    m_groupBox = new QGroupBox();
    vlayout->addWidget(new QWidget());
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(m_groupBox);
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(new QWidget());

    m_slider = new QnSettingsSlider(Qt::Horizontal, m_groupBox);

    m_slider->setRange(m_param.getMin(), m_param.getMax());
    m_slider->setValue(m_param.getCurrent());
    m_slider->setWindowTitle(m_param.getName() + QLatin1String(" (") + (QString)m_param.getCurrent() + QLatin1Char(')'));

    m_groupBox->setTitle(m_param.getName() + QLatin1String(" (") + (QString)m_param.getCurrent() + QLatin1Char(')'));

    QVBoxLayout *layout = new QVBoxLayout(m_groupBox);
    layout->addWidget(m_slider);

    connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onValChanged()));
    connect(m_slider, SIGNAL(onKeyReleased()), this, SLOT(onValChanged()));
    connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onValChanged(int)) );
}

void QnSettingsMinMaxStepWidget::refresh()
{
    m_slider->setRange(m_param.getMin(), m_param.getMax());
    m_slider->setValue(m_param.getCurrent());
    m_slider->setWindowTitle(m_param.getName() + QLatin1String(" (") + (QString)m_param.getCurrent() + QLatin1Char(')'));

    m_groupBox->setTitle(m_param.getName() + QLatin1String(" (") + (QString)m_param.getCurrent() + QLatin1Char(')'));
}

void QnSettingsMinMaxStepWidget::onValChanged(int val)
{
    m_groupBox->setTitle(m_param.getName() + QLatin1String(" (") + QString::number(val) + QLatin1Char(')'));
}

void QnSettingsMinMaxStepWidget::onValChanged()
{
    setParam(m_slider->value());
}

void QnSettingsMinMaxStepWidget::updateParam(QString val)
{
    CameraSettingValue cv(val);
    m_slider->setValue(cv);
}

//==============================================
QnSettingsEnumerationWidget::QnSettingsEnumerationWidget(const CameraSetting &obj, QnSettingsScrollArea *parent):
    QnAbstractSettingsWidget(obj, parent, obj.getDescription())
{
    QVBoxLayout *vlayout = new QVBoxLayout();
    m_layout->addLayout(vlayout);

    QGroupBox* groupBox = new QGroupBox();
    vlayout->addWidget(new QWidget());
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(groupBox);
    vlayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    vlayout->addWidget(new QWidget());

    groupBox->setTitle(m_param.getName());

    QVBoxLayout *layout = new QVBoxLayout(groupBox);

    QStringList values = static_cast<QString>(m_param.getMin()).split(QLatin1Char(','));
    for (int i = 0; i < values.length(); ++i)
    {
        QString val = values[i].trimmed();

        QRadioButton *btn = new QRadioButton(val, groupBox);
        layout->addWidget(btn);

        if (val == m_param.getCurrent())
            btn->setChecked(true);

        btn->setObjectName(val);
    
        m_radioBtns.push_back(btn);

        connect(btn , SIGNAL(clicked()), this, SLOT(onClicked()));
    }
}

void QnSettingsEnumerationWidget::refresh()
{
    QStringList values = static_cast<QString>(m_param.getMin()).split(QLatin1Char(','));
    for (int i = 0; i < values.length(); ++i)
    {
        QString val = values[i].trimmed();
        QRadioButton *btn = m_radioBtns.at(i);

        if (val == m_param.getCurrent()) {
            btn->setChecked(true);
        } else {
            btn->setChecked(false);
        }
    }
}

void QnSettingsEnumerationWidget::onClicked()
{
    QString val = QObject::sender()->objectName();

    setParam(val);
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
QnSettingsButtonWidget::QnSettingsButtonWidget(const CameraSetting &obj, QnSettingsScrollArea *parent):
    QnAbstractSettingsWidget(obj, parent, obj.getDescription())
{
    QPushButton* btn = new QPushButton(m_param.getName());
    m_layout->addWidget(new QWidget());
    m_layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    m_layout->addWidget(btn);
    m_layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    m_layout->addWidget(new QWidget()); // TODO: #Elric hueta

    QObject::connect(btn, SIGNAL(released()), this, SLOT(onClicked()));

    btn->setFocusPolicy(Qt::NoFocus);
}

void QnSettingsButtonWidget::refresh()
{
}

void QnSettingsButtonWidget::onClicked()
{
    setParam("");
}

void QnSettingsButtonWidget::updateParam(QString /*val*/)
{
    
}

//==============================================
QnSettingsTextFieldWidget::QnSettingsTextFieldWidget(const CameraSetting &obj, QnSettingsScrollArea *parent):
    QnAbstractSettingsWidget(obj, parent, obj.getDescription())
{
    m_lineEdit = new QLineEdit();
    m_lineEdit->setText(obj.getCurrent());

    m_layout->addWidget(new QWidget());
    m_layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    m_layout->addWidget(m_lineEdit);
    m_layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Maximum, QSizePolicy::Expanding));
    m_layout->addWidget(new QLabel(obj.getName()));

    m_lineEdit->setReadOnly( obj.isReadOnly() );

    connect( m_lineEdit, &QLineEdit::editingFinished, this, &QnSettingsTextFieldWidget::onChange );
}

void QnSettingsTextFieldWidget::onChange()
{
    if( !m_lineEdit->isModified() )
        return;
    setParam( m_lineEdit->text() );
    m_lineEdit->setModified( false );
}

void QnSettingsTextFieldWidget::refresh()
{
    m_lineEdit->setText(m_param.getCurrent());
}

void QnSettingsTextFieldWidget::updateParam(QString val)
{
    Q_UNUSED(val)
}

//==============================================
QnSettingsControlButtonsPairWidget::QnSettingsControlButtonsPairWidget(const CameraSetting &obj, QnSettingsScrollArea *parent):
    QnAbstractSettingsWidget(obj, parent, obj.getDescription())
{
    QHBoxLayout *hlayout = new QHBoxLayout();
    m_layout->addLayout(hlayout);

    m_minBtn = new QPushButton(lit("-"));
    m_minBtn->setFocusPolicy(Qt::NoFocus);
    m_maxBtn = new QPushButton(lit("+"));
    m_maxBtn->setFocusPolicy(Qt::NoFocus);

    hlayout->addStretch(1);
    hlayout->addWidget(new QLabel(obj.getName()));
    hlayout->addWidget(m_minBtn);
    hlayout->addWidget(m_maxBtn);
    hlayout->addStretch(1);

    QObject::connect(m_minBtn, SIGNAL(pressed()), this, SLOT(onMinPressed()));
    QObject::connect(m_minBtn, SIGNAL(released()), this, SLOT(onMinReleased()));

    QObject::connect(m_maxBtn, SIGNAL(pressed()), this, SLOT(onMaxPressed()));
    QObject::connect(m_maxBtn, SIGNAL(released()), this, SLOT(onMaxReleased()));
}

void QnSettingsControlButtonsPairWidget::onMinPressed()
{
    setParam(m_param.getMin());
}

void QnSettingsControlButtonsPairWidget::onMinReleased()
{
    setParam(m_param.getStep());
}

void QnSettingsControlButtonsPairWidget::onMaxPressed()
{
    setParam(m_param.getMax());
}

void QnSettingsControlButtonsPairWidget::onMaxReleased()
{
    setParam(m_param.getStep());
}

void QnSettingsControlButtonsPairWidget::refresh()
{
    
}

void QnSettingsControlButtonsPairWidget::updateParam(QString val)
{
    Q_UNUSED(val)
}
