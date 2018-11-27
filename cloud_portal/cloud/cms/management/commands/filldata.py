from django.core.management.base import BaseCommand
from ...controllers import filldata
from ...models import Customization
from cloud import settings


class Command(BaseCommand):
    help = 'Fills initial data from CMS database to static files'

    def add_arguments(self, parser):
        parser.add_argument('--customization', nargs='?', default=settings.CUSTOMIZATION, type=str)
        parser.add_argument('--preview', nargs='?', default=False, type=bool)

    def handle(self, *args, **options):
        if 'customization' not in options:
            filldata.init_skin(settings.CUSTOMIZATION, preview=options['preview'])
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + settings.CUSTOMIZATION))
        elif options['customization'] == 'all':
            for custom in Customization.objects.all():
                filldata.init_skin(custom.name, preview=options['preview'])
                self.stdout.write(self.style.SUCCESS('Initiated static content for ' + custom.name))
        else:
            filldata.init_skin(options['customization'], preview=options['preview'])
            self.stdout.write(self.style.SUCCESS(
                'Initiated static content for ' + options['customization']))

        self.stdout.write(self.style.SUCCESS('Successfully initiated static content'))
