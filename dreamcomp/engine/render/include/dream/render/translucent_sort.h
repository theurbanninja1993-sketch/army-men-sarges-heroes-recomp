#pragma once
#include <cstddef>
#include <vector>
#include "dream/render/display_list.h"
namespace dream::render {
// Refine an existing back-to-front order using depth over the actual screen
// overlap. Intersecting/coplanar pairs retain the fallback order.
void order_translucent_overlaps(const Frame& frame, std::vector<const Polygon*>& polygons);
}
