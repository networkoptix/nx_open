# -*- coding: utf-8 -*-
# Generated by Django 1.10.2 on 2017-09-28 02:24
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('cms', '0019_auto_20170926_2159'),
    ]

    operations = [
        migrations.AddField(
            model_name='context',
            name='template',
            field=models.TextField(blank=True, default=''),
        ),
        migrations.AlterField(
            model_name='context',
            name='description',
            field=models.TextField(blank=True, default=''),
        ),
    ]
