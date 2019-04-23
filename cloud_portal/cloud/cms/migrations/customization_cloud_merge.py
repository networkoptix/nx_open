# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.db import migrations, models

# TODO: removed in 19.1


class Migration(migrations.Migration):

    dependencies = [
        ('cms', '0036_convert_to_utf8mb4'),
    ]

    operations = [
        migrations.AddField(
            model_name='customization',
            name='reveal_cloud_merge',
            field=models.BooleanField(default=False, help_text="Shows the cloud merge button for all systems."),
        ),
    ]
