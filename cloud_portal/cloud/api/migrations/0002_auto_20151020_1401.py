# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('api', '0001_initial'),
    ]

    operations = [
        migrations.AlterField(
            model_name='account',
            name='activated_date',
            field=models.DateField(null=True, blank=True),
        ),
        migrations.AlterField(
            model_name='account',
            name='last_login',
            field=models.DateField(null=True, blank=True),
        ),
    ]
