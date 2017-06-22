from __future__ import absolute_import
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny, IsAdminUser
from rest_framework.response import Response
from django.core.exceptions import PermissionDenied
from django.shortcuts import render
from cloud import settings

from .forms import *

# Create your views here.
@api_view(["GET", "POST"])
def page_edit_view(request):
	form = CustomForm()
	data_structures = []
	if request.method == "POST":
		context_id = request.data['context']
		context = Context.objects.get(id=context_id)
		language_id = request.data['language']
		language = Language.objects.get(id=language_id)
		
		if 'GetDataStructures' in request.data:
			form.add_fields(context)

		elif 'Save' in request.data:
			for data_structure in context.datastructure_set.all():
				data_structure_name = data_structure.name

				if data_structure_name in request.data and request.data[data_structure_name]:
					record = DataRecord(data_structure=data_structure,
										language=language,
										customization=settings.CUSTOMIZATION,
										value=request.data[data_structure_name])
					record.save()



	return render(request, 'page_edit_form.html', {'form': form, 'data_structures': data_structures})