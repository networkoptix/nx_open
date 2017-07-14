from datetime import datetime

from notifications.engines.email_engine import send 

from .filldata import fill_content
from api.models import Account
from cms.models import *

def get_context_and_language(request_data, context_id, language_id):
	context = Context.objects.get(id=context_id) if context_id else None
	language = Language.objects.get(id=language_id) if language_id else None
	
	if not context and 'context' in request_data and request_data['context']:
		context = Context.objects.get(id=request_data['context'])
	
	if not language and 'language' in request_data and request_data['language']: 
		language = Language.objects.get(id=request_data['language'])

	return context, language


def accept_latest_draft(customization, user):
	unaccepted_version = ContentVersion.objects.filter(accepted_date=None, customization=customization).latest('created_date')
	unaccepted_version.accepted_by = user
	unaccepted_version.accepted_date = datetime.now()
	unaccepted_version.save()


def notify_version_ready(customization, version_id, product_name):
	super_users = Account.objects.filter(customization=customization, is_superuser=1)
	for user in super_users:
		send(user.email, "review_version", {'id':version_id, 'product': product_name}, settings.CUSTOMIZATION)


def save_unrevisioned_records(customization, language, data_structures, request_data, user):
	for data_structure in data_structures:
		data_structure_name = data_structure.name
		
		latest_unapproved_record = data_structure.datarecord_set.filter(customization=customization,
																		language=language,
																		version=None)
		
		latest_approved_record = data_structure.datarecord_set.filter(customization=customization,
																	  language=language)\
															  .exclude(version=None)
		
		new_record_value = request_data[data_structure_name]

		if latest_unapproved_record.exists():
			if new_record_value == latest_unapproved_record.latest('created_date').value:
			   	continue
		elif latest_approved_record.exists():
			if new_record_value == latest_approved_record.latest('created_date').value:
				continue
		elif data_structure.default == new_record_value:
			continue

		record = DataRecord(data_structure=data_structure,
							language=language,
							customization=customization,
							value=new_record_value,
							created_by=user)
		record.save()


def alter_records_version(contexts, customization, old_version, new_version):
	languages = Language.objects.all()
	for context in contexts:
		for data_structure in context.datastructure_set.all():
			record = data_structure.datarecord_set.filter(customization=customization,
														   version=old_version)

			#Now only the latest records that can be published will have its version altered
			if record.exists():
				latest_record = record.latest('created_date')
				latest_record.version = new_version
				latest_record.save()


def generate_preview(context=None):
	fill_content(customization_name=settings.CUSTOMIZATION)
	return '/' + context.url + "?preview" if context else "/?preview"


def publish_latest_version(customization, user):
	accept_latest_draft(customization, user)
	fill_content(customization_name=settings.CUSTOMIZATION, preview=False)


def send_version_for_review(customization, language, data_structures, product, request_data, user):
	old_versions = ContentVersion.objects.filter(accepted_date=None)

	if old_versions.exists():
		old_version = old_versions.latest('created_date')
		alter_records_version(Context.objects.filter(product=product), customization, old_version, None)
		old_version.delete()

	save_unrevisioned_records(customization, language, data_structures, request_data, user)

	version = ContentVersion(customization=customization, name="N/A", created_by=user)
	version.save()

	alter_records_version(Context.objects.filter(product=product), customization, None, version)
	#TODO add notification need to make template for this
	notify_version_ready(customization, version.id, product.name)

def get_records_for_version(version):
	data_records = version.datarecord_set.all().order_by('data_structure__context__name',
														 'language__code')
	contexts = {}

	for record in data_records:
		context_name = record.data_structure.context.name
		if  context_name in contexts:
			contexts[context_name].append(record)
		else:
			contexts[context_name] = [record]
	return contexts