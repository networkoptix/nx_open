# -*- coding: utf-8 -*-
# Generated by Django 1.10.2 on 2018-12-13 21:52
from __future__ import unicode_literals

from django.db import migrations


class Migration(migrations.Migration):

    dependencies = [
        ('cms', '0049_auto_20181207_0316'),
    ]

    operations = [
        migrations.DeleteModel(
            name='ContextProxy',
        ),
        migrations.AlterModelOptions(
            name='context',
            options={'permissions': (('edit_content', 'Can edit content and send for review'),), 'verbose_name': 'page', 'verbose_name_plural': 'pages'},
        ),
        migrations.AlterModelOptions(
            name='customization',
            options={'permissions': (('can_view_customization', 'Can view customization'),)},
        ),
    ]
