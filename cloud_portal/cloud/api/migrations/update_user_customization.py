from __future__ import unicode_literals

from django.db import connection, migrations


#migratedb shouldn't assume cloud_db database exists at the moment
#currently not in use do nothing


#def update_customizations_from_clouddb(apps, schema_editor):
#    with connection.cursor() as cursor:
#        cursor.execute('UPDATE cloudportal.api_account db1 JOIN clouddb.account db2 ON db1.email=db2.email SET db1.customization=db2.customization')

#class Migration(migrations.Migration):

#    dependencies = [
#        ('api', '0012_account_customization'),
#    ]

#    operations = [
#        migrations.RunPython(update_customizations_from_clouddb),
#    ]
