# -*- coding: utf-8 -*-
# Generated by Django 1.10.2 on 2018-11-13 00:38
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('api', '0020_auto_20181108_0232'),
    ]

    operations = [
        migrations.AlterField(
            model_name='account',
            name='is_active',
            field=models.BooleanField(default=True, help_text=b'If false the account is disabled. <br> If user was invited to the cloud it will switch to true on its own when the user completes registration.'),
        ),
        migrations.AlterField(
            model_name='account',
            name='is_staff',
            field=models.BooleanField(default=False, help_text=b'If true then the user can view cloud admin.'),
        ),
    ]
