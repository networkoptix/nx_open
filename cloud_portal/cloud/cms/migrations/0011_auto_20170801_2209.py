# -*- coding: utf-8 -*-
# Generated by Django 1.10.2 on 2017-08-01 22:09
from __future__ import unicode_literals

from django.db import migrations, models
import django.db.models.deletion


class Migration(migrations.Migration):

    dependencies = [
        ('cms', '0010_auto_20170719_1632'),
    ]

    operations = [
        migrations.AlterField(
            model_name='datarecord',
            name='language',
            field=models.ForeignKey(blank=True, null=True, on_delete=django.db.models.deletion.CASCADE, to='cms.Language'),
        ),
    ]
