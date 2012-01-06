/*
 *  property_animation.h
 *  uniclient
 *
 *  Created by Ivan Vigasin on 6/21/11.
 *  Copyright 2011 Network Optix. All rights reserved.
 *
 */

#ifndef PROPERTY_ANIMATION_H
#define PROPERTY_ANIMATION_H

#include <QtCore/QPropertyAnimation>

/**
  * @class AnimationManager
  *
  * Manage concurrent animation to not allow multiple repaints
  */
class AnimationManager
{
public:
    static QPropertyAnimation *addAnimation(QObject *target, const QByteArray &propertyName, QObject *parent = 0);
};

#endif // PROPERTY_ANIMATION_H
