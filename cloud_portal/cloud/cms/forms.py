from django import forms
from .models import *

class CustomForm(forms.ModelForm):
	#product = forms.ModelChoiceField(widget=forms.Select, label="Product Name", queryset=Product.objects.all())
	context = forms.ModelChoiceField(widget=forms.Select, label="Context", queryset=Context.objects.all())
	language = forms.ModelChoiceField(widget=forms.Select, label="Language", queryset=Language.objects.all())

	class Meta():
		fields = "__all__"
		model = Blank

	def add_fields(self, context):
		data_structures = context.datastructure_set.all()
		
		if len(data_structures) < 1:
			return
		
		for data_structure in data_structures:
			ds_name = data_structure.name
			ds_description = data_structure.description
			latest_record = data_structure.datarecord_set.latest('created_date')
			record_value = latest_record.value

			self.fields[ds_name] = forms.CharField(widget=forms.Textarea(attrs={'placeholder':record_value}),
												   required=False,
												   label=ds_name,
												   help_text=ds_description)

class ContextForm(forms.ModelForm):

	class Meta():
		fields = "__all__"
		model = DataStructure