# -*- coding: utf-8 -*-
# Generated by Django 1.10.2 on 2017-09-19 19:14
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('cms', '0012_auto_20170802_1950'),
    ]

    operations = [
        migrations.AddField(
            model_name='datastructure',
            name='label',
            field=models.CharField(default=None, max_length=1024),
            preserve_default=False,
        ),
    ]
