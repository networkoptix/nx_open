from rest_framework import serializers
from cms.models import Context, DataStructure, ProductType


class DataStructureSerializer(serializers.ModelSerializer):
    class Meta:
        model = DataStructure
        fields = ("label", "name", "value", "description", "type", "advanced", "optional", "public", "meta")
        ordering = ('order', )

    value = serializers.CharField(source="default")
    meta = serializers.JSONField(source="meta_settings")
    type = serializers.SerializerMethodField("get_nice_name")

    def get_nice_name(self, obj):
        return DataStructure.DATA_TYPES[obj.type]


class ContextSerializer(serializers.ModelSerializer):
    class Meta:
        model = Context
        fields = ("name", "label", "file_path", "description", "url", "translatable", "values")

    values = DataStructureSerializer(source="datastructure_set", many=True)


class ProductTypeSerializer(serializers.ModelSerializer):
    class Meta:
        model = ProductType
        fields = ("type", "can_preview", "single_customization", "contexts")

    contexts = ContextSerializer(source="context_set", many=True)
    type = serializers.SerializerMethodField("get_nice_name")

    def get_nice_name(self, obj):
        return ProductType.PRODUCT_TYPES[obj.type]



