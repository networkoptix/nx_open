from django.core.management.base import BaseCommand

from django.db import DEFAULT_DB_ALIAS, connections
from django.db.migrations.executor import MigrationExecutor


class Command(BaseCommand):
    help = "Check if migrations are done."

    def handle(self, *args, **options):
        executor = MigrationExecutor(connections[DEFAULT_DB_ALIAS])
        plan = executor.migration_plan(executor.loader.graph.leaf_nodes())
        if not plan:
            self.stdout.write(self.style.SUCCESS("Done"))
