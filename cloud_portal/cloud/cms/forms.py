from django import forms
from .models import *

class CustomContextForm(forms.ModelForm):
	language = forms.ModelChoiceField(widget=forms.Select, label="Language", queryset=Language.objects.all())

	class Meta():
		fields = "__all__"
		model = Blank

	def remove_langauge(self):
		super(CustomContextForm, self)
		self.fields.pop('language')

	def add_fields(self, context, language):
		data_structures = context.datastructure_set.all()
		
		if len(data_structures) < 1:
			return

		if not context.translatable:
			self.remove_langauge()
		
		for data_structure in data_structures:
			ds_name = data_structure.name
			
			ds_description = data_structure.description

			latest_record = data_structure.datarecord_set.filter(language=language)

			record_value = latest_record.latest('created_date').value if latest_record.exists() else data_structure.default

			if data_structure.type == 3:
				self.fields[ds_name] = forms.CharField(required=False,
													   label=ds_name,
													   help_text=ds_description,
													   initial=record_value,
													   widget=forms.Textarea)
			else:
				self.fields[ds_name] = forms.CharField(required=False,
												   	   label=ds_name,
												   	   help_text=ds_description,
												   	   initial=record_value,
												   	   widget=forms.TextInput(attrs={'size':80}))