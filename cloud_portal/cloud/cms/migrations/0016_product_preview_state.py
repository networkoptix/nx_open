# -*- coding: utf-8 -*-
# Generated by Django 1.10.2 on 2017-09-20 01:23
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('cms', '0015_datastructure_advanced'),
    ]

    operations = [
        migrations.AddField(
            model_name='product',
            name='preview_state',
            field=models.CharField(choices=[('draft', 'draft'), ('review', 'review')], default='draft', max_length=20),
        ),
    ]
