# -*- coding: utf-8 -*-
from __future__ import unicode_literals

from django.db import migrations, models
import jsonfield.fields


class Migration(migrations.Migration):

    dependencies = [
    ]

    operations = [
        migrations.CreateModel(
            name='Message',
            fields=[
                ('id', models.AutoField(verbose_name='ID', serialize=False, auto_created=True, primary_key=True)),
                ('user_email', models.CharField(max_length=255)),
                ('type', models.CharField(max_length=255)),
                ('message', jsonfield.fields.JSONField()),
                ('created_date', models.DateField(auto_now_add=True)),
                ('send_date', models.DateField(null=True, blank=True)),
            ],
        ),
    ]
