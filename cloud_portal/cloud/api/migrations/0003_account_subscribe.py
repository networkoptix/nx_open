# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ('api', '0002_auto_20151020_1401'),
    ]

    operations = [
        migrations.AddField(
            model_name='account',
            name='subscribe',
            field=models.BooleanField(default=False),
        ),
    ]
