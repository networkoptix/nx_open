#include "model_functions.h"

namespace QnModelFunctionsDetail {
  bool equals(float l, float r) { return qFuzzyCompare(l, r); }
  bool equals(double l, double r) { return qFuzzyCompare(l, r); }
}
