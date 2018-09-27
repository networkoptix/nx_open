from __future__ import unicode_literals

from cms import models


class ProductCustomizationReview(models.ProductCustomizationReview):
    class Meta:
        proxy = True
        verbose_name = "Product Review"
        verbose_name_plural = "Product Reviews"
