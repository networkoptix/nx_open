from datetime import datetime

from notifications.engines.email_engine import send

from PIL import Image
import base64, ast

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


def accept_latest_draft(user):
	unaccepted_version = ContentVersion.objects.filter(accepted_date=None).latest('created_date')
	unaccepted_version.accepted_by = user
	unaccepted_version.accepted_date = datetime.now()
	unaccepted_version.save()


def notify_version_ready(version_id, product_name):
	super_users = Account.objects.filter(is_superuser=1)
	for user in super_users:
		send(user.email, "review_version", {'id':version_id, 'product': product_name}, settings.CUSTOMIZATION)


def save_unrevisioned_records(customization, language, data_structures, request_data, request_files, user):
	upload_errors = []
	for data_structure in data_structures:
		data_structure_name = data_structure.name
		
		latest_unapproved_record = data_structure.datarecord_set.filter(customization=customization,
																		language=language,
																		version=None)
		
		latest_approved_record = data_structure.datarecord_set.filter(customization=customization,
																	  language=language)\
															  .exclude(version=None)
		
		new_record_value = None
		#If the DataStructure is supposed to be an image convert to base64 and error check
		if data_structure_name in request_files:
			new_record_value, dimensions, invalid_file_type = handle_image_upload(request_files[data_structure_name])

			if invalid_file_type:
				upload_errors.append((data_structure_name, 'Invalid file type. Can only upload ".png"'))
				continue

			#Gets the meta_settings form the DataStructure to check if the sizes are valid
			data_structure_meta_string = data_structure.meta_settings
			#ast.literal_eval used to convert string to dict
			data_structure_meta = ast.literal_eval(data_structure_meta_string)

			if data_structure_meta['height'] < dimensions['height'] or\
			   data_structure_meta['width'] < dimensions['width']:
			   	
			   	size_error_msg = 'Size is too big. Height must be less than {}. Width must be less than {}.'\
			   						.format(data_structure_meta['height'], data_structure_meta['width'])
				
				upload_errors.append((data_structure_name, size_error_msg))
				continue
		else:
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

	return upload_errors


def alter_records_version(contexts, customization, old_version, new_version):
	languages = Language.objects.all()
	for context in contexts:
		for data_structure in context.datastructure_set.all():
			records = data_structure.datarecord_set.filter(customization=customization,
														   version=old_version)
			for record in records:
				record.version = new_version
				record.save()


def generate_preview(context=None):
	fill_content(customization_name=settings.CUSTOMIZATION)
	return '/' + context.url + "?preview" if context else "/?preview"


def publish_latest_version(user):
	accept_latest_draft(user)
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
	notify_version_ready(version.id, product.name)


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


def handle_image_upload(image):
	encoded_string = base64.b64encode(image.read())
	file_type = image.content_type

	if file_type != 'image/png':
		return None, None, True

	newImage = Image.open(image)
	width, height = newImage.size
	return encoded_string, {'width': width, 'height': height}, False