from django import forms
from .models import *

class CustomForm(forms.ModelForm):
	#product = forms.ModelChoiceField(widget=forms.Select, label="Product Name", queryset=Product.objects.all())
	context = forms.ModelChoiceField(widget=forms.Select, label="Context", queryset=Context.objects.all())

	#fieldsets = [(None, {'fields': ['ex_model','product','context']}),]

	class Meta():
		fields = "__all__"
		model = Blank

	def add_fields(self, context):
		children = context.datastructure_set.all()
		
		if len(children) < 1:
			return
		DATA_TYPES = ((0, 'Text'),(1, 'Image'),)
		
		for child in children:
			child_name = 'Information for ' + str(child.name)
			child_description = str(child.description)
			self.fields[child_name] = forms.CharField(widget=forms.Textarea, required=False, label=child_name, help_text=child_description)

class ContextForm(forms.ModelForm):

	class Meta():
		fields = "__all__"
		model = DataStructure