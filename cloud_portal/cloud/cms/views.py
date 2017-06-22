from __future__ import absolute_import
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny, IsAdminUser
from rest_framework.response import Response
from django.core.exceptions import PermissionDenied
from django.shortcuts import render

from .forms import *

# Create your views here.
@api_view(["GET", "POST"])
def model_list_view(request):
	form = CustomForm()
	data_structures = []
	if request.method == "POST":
		context_id = request.data['context']
		context = Context.objects.get(id=context_id)
		
		if 'Get Data Structures' in request.data:
			form.add_fields(context)

		elif 'Save' in request.data:
			language = Language.objects.get()
			customization = Customization.objects.get()
			for data_structure in context.datastructure_set.all():
				data_structure_name = str(data_structure.name)

				if data_structure_name in request.data and request.data[data_structure_name]:
					record = DataRecord(data_structure=data_structure, language=language, 
										customization=customization, value=request.data[data_structure_name])
					record.save()



	return render(request, 'custom_form.html', {'form': form, 'data_structures': data_structures})