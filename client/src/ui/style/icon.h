#ifndef QN_ICON_H
#define QN_ICON_H

class QnIcon: public QIcon {
public:
    static constexpr QIcon::Mode Pressed = static_cast<QIcon::Mode>(0xF);
};

#endif // QN_ICON_H
