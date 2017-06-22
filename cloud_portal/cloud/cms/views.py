from __future__ import absolute_import
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny, IsAdminUser
from rest_framework.response import Response
from django.core.exceptions import PermissionDenied
from django.shortcuts import render
from cloud import settings

from .forms import *


def update_data_records_for_context(language, data_structures, request_data):
	customization = Customization.objects.get(name=settings.CUSTOMIZATION)
	for data_structure in data_structures:
		data_structure_name = data_structure.name
		latest_record = data_structure.datarecord_set.latest('created_date').value
		
		if data_structure_name in request_data and request_data[data_structure_name] != latest_record:
			new_record = request_data[data_structure_name]
			
			record = DataRecord(data_structure=data_structure,
								language=language,
								customization=customization,
								value=new_record)
			record.save()

# Create your views here.
@api_view(["GET", "POST"])
def page_edit_view(request):
	form = CustomForm()
	if request.method == "POST":
		context_id = request.data['context']
		context = Context.objects.get(id=context_id)
		language_id = request.data['language']
		language = Language.objects.get(id=language_id)
		
		if 'GetDataStructures' in request.data:
			form.add_fields(context)

		elif 'Save' in request.data:
			update_data_records_for_context(language, context.datastructure_set.all(), request.data)


	return render(request, 'page_edit_form.html', {'form': form})
