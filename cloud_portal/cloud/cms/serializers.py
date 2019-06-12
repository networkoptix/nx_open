from rest_framework import serializers
from cms.models import Context, DataStructure, ProductType


class BaseCMSSerializer(serializers.ModelSerializer):
    def __init__(self, *args, **kwargs):
        self.product_id = 0
        if "product_id" in kwargs:
            self.product_id = kwargs.pop("product_id")
        super().__init__(*args, **kwargs)


class DataStructureSerializer(BaseCMSSerializer):
    class Meta:
        model = DataStructure
        fields = ("label", "name", "value", "description", "type", "advanced", "optional", "public", "meta")
        ordering = ('order', )

    value = serializers.SerializerMethodField('get_value_for_datastructure')
    meta = serializers.JSONField(source="meta_settings")
    type = serializers.SerializerMethodField("get_nice_name")

    def get_value_for_datastructure(self, obj):
        if obj.type in [DataStructure.DATA_TYPES.image, DataStructure.DATA_TYPES.file]:
            return ""
        if self.product_id:
            current_record = obj.datarecord_set.filter(product_id=self.product_id).last()
            if current_record:
                return current_record.value
        return obj.default

    def get_nice_name(self, obj):
        return DataStructure.DATA_TYPES[obj.type]


class ContextSerializer(BaseCMSSerializer):
    class Meta:
        model = Context
        fields = ("name", "label", "file_path", "description", "url", "translatable", "values")

    values = serializers.SerializerMethodField('get_datastructure_values')

    def get_datastructure_values(self, obj):
        return DataStructureSerializer(obj.datastructure_set.all(), many=True, product_id=self.product_id).data


class ProductTypeSerializer(BaseCMSSerializer):
    class Meta:
        model = ProductType
        fields = ("type", "can_preview", "single_customization", "contexts")

    contexts = serializers.SerializerMethodField('get_contexts_values')
    type = serializers.SerializerMethodField("get_nice_name")

    def get_contexts_values(self, obj):
        return ContextSerializer(obj.context_set.all(), many=True, product_id=self.product_id).data

    def get_nice_name(self, obj):
        return ProductType.PRODUCT_TYPES[obj.type]
