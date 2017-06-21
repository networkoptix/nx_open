from __future__ import absolute_import
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny, IsAdminUser
from rest_framework.response import Response
from django.core.exceptions import PermissionDenied
from django.shortcuts import render

from .forms import *

preModel = {'Product': ('name',),
			'Context': ('product', 'name', 'description', 'translatable', 'file_path', 'url'),
			'DataStructure': ('context', 'name', 'description', 'type', 'default', 'translatable'),
			'Language': ('name','code'),
			'Customization': ('name', 'default_language'),
			'ContentVersion': ('customization', 'name', 'created_date', 'created_by', 'accepted_date', 'accepted_by'),
			'DataRecord': ('data_structure', 'language', 'customization', 'version', 'created_date', 'created_by', 'value')
}

# Create your views here.
@api_view(["GET", "POST"])
def model_list_view(request):
	form = CustomForm()
	data_structures = []
	if request.method == "POST" and 'Get Data Structures' in request.data:
		context_id = request.data['context']
		context = Context.objects.get(id=context_id)
		form.add_fields(context)



	return render(request, 'custom_form.html', {'form': form, 'data_structures': data_structures})