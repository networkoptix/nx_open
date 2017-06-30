from datetime import datetime

from notifications.tasks import send_email

from api.models import Account
from cms.models import *

def get_post_request_params(request_data): 
	return Context.objects.get(id=request_data['context']), Language.objects.get(id=request_data['language'])


def accept_latest_draft(user):
	unaccepted_version = ContentVersion.objects.filter(accepted_date=None).latest('created_date')
	unaccepted_version.accepted_by = user
	unaccepted_version.accepted_date = datetime.now()
	unaccepted_version.save()


def notify_version_ready():
	super_users = Account.objects.filter(is_superuser)
	for user in super_users:
		send_email(user.email, "version_ready_to_publish","",settings.CUSTOMIZATION)


def save_unrevisioned_records(customization, language, data_structures, request_data, user):
	for data_structure in data_structures:
		data_structure_name = data_structure.name
		
		latest_unapproved_record = data_structure.datarecord_set.filter(customization=customization, language=language, version=None).exclude(created_by=None)
		
		latest_approved_record = data_structure.datarecord_set.filter(customization=customization,language=language).exclude(version=None, created_by=None)
		
		new_record_value = request_data[data_structure_name]

		if latest_unapproved_record.exists():
			latest_unapproved_record = latest_unapproved_record.latest('created_date')
			if new_record_value == latest_unapproved_record.value:
			   	continue
			latest_unapproved_record.delete()
		
		elif latest_approved_record:
			if latest_approved_record.latest('created_date').value == new_record_value:
				continue
		elif not new_record_value:
			continue

		record = DataRecord(data_structure=data_structure,
							language=language,
							customization=customization,
							value=new_record_value,
							created_by=user)
		record.save()


def remove_links_to_old_version(old_version, customization, contexts):
	for context in contexts:
		for data_structure in context.datastructure_set.all():
			for record in data_structure.datarecord_set.filter(customization=customization, version=old_version):
				record.version = None
				record.save()

	old_version.delete()


def alter_records_version(contexts, customization, old_version, new_version):
	for context in contexts:
		for data_structure in context.datastructure_set.all():
			records = data_structure.datarecord_set.filter(customization=customization, version=old_version).exclude(created_by=None)
			for record in records:
				record.version = new_version
				record.save()
