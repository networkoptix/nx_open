from django import forms
from .models import *

class CustomContextForm(forms.ModelForm):
	#product = forms.ModelChoiceField(widget=forms.Select, label="Product Name", queryset=Product.objects.all())
	context = forms.ModelChoiceField(widget=forms.Select, label="Context", queryset=Context.objects.all())
	language = forms.ModelChoiceField(widget=forms.Select, label="Language", queryset=Language.objects.all())

	class Meta():
		fields = "__all__"
		model = Blank

	def add_fields(self, context, language):
		data_structures = context.datastructure_set.all()
		
		if len(data_structures) < 1:
			return
		
		for data_structure in data_structures:
			ds_name = data_structure.name
			
			ds_description = data_structure.description

			latest_record = data_structure.datarecord_set.filter(language=language)

			record_value = latest_record.latest('created_date').value if latest_record.exists() else ""

			self.fields[ds_name] = forms.CharField(widget=forms.Textarea,
												   required=False,
												   label=ds_name,
												   help_text=ds_description,
												   initial=record_value)