# -*- coding: utf-8 -*-
# Generated by Django 1.10.2 on 2018-10-17 01:33
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('cms', '0043_auto_20181016_2056'),
    ]

    operations = [
        migrations.AddField(
            model_name='datarecord',
            name='file',
            field=models.FileField(blank=True, default=None, upload_to=''),
        ),
        migrations.AlterField(
            model_name='datarecord',
            name='value',
            field=models.TextField(blank=True, default=''),
        ),
    ]
