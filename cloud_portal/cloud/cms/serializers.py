from rest_framework import serializers
from cms.models import Context, DataStructure, ProductType


class BaseCMSSerializer(serializers.ModelSerializer):
    def __init__(self, *args, **kwargs):
        self.query = {}
        use_actual_values = False
        if "use_actual_values" in kwargs:
            use_actual_values = kwargs.pop("use_actual_values")

        if "product" in kwargs:
            self.query["product"] = kwargs.pop("product") and use_actual_values
        if "lang" in kwargs:
            self.query["lang"] = kwargs.pop("lang") and use_actual_values
        if "draft" in kwargs:
            self.query["draft"] = kwargs.pop("draft") and use_actual_values

        if "params" in kwargs:
            self.query.update(kwargs.pop("params"))

        super().__init__(*args, **kwargs)


class DataStructureSerializer(BaseCMSSerializer):
    class Meta:
        model = DataStructure
        fields = ("label", "name", "value", "description", "type", "advanced", "optional", "public", "meta")

    value = serializers.SerializerMethodField('get_value_for_datastructure')
    meta = serializers.JSONField(source="meta_settings")
    type = serializers.SerializerMethodField("get_nice_name")

    def get_value_for_datastructure(self, obj):
        if self.query:
            if obj.type in [DataStructure.DATA_TYPES.image, DataStructure.DATA_TYPES.file]:
                return ""

            return obj.find_actual_value(product=self.query["product"],
                                         language=self.query["lang"],
                                         draft=self.query["draft"])
        return obj.default

    def get_nice_name(self, obj):
        return DataStructure.DATA_TYPES[obj.type]


class ContextSerializer(BaseCMSSerializer):
    class Meta:
        model = Context
        fields = ("name", "label", "file_path", "description", "url", "translatable", "values")

    values = serializers.SerializerMethodField('get_datastructure_values')

    def get_datastructure_values(self, obj):
        return DataStructureSerializer(obj.datastructure_set.all(), many=True, params=self.query).data


class ProductTypeSerializer(BaseCMSSerializer):
    class Meta:
        model = ProductType
        fields = ("type", "can_preview", "single_customization", "contexts")

    contexts = serializers.SerializerMethodField('get_contexts_values')
    type = serializers.SerializerMethodField("get_nice_name")

    def get_contexts_values(self, obj):
        return ContextSerializer(obj.context_set.all(), many=True, params=self.query).data

    def get_nice_name(self, obj):
        return ProductType.PRODUCT_TYPES[obj.type]
