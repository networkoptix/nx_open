# -*- coding: utf-8 -*-
# Generated by Django 1.10.2 on 2018-08-21 01:13
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('cms', '0033_customization_public_downloads'),
    ]

    operations = [
        migrations.AddField(
            model_name='contexttemplate',
            name='skin',
            field=models.CharField(blank=True, default=b'blue', max_length=16),
        ),
    ]
