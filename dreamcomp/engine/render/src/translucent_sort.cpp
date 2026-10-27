#include "dream/render/translucent_sort.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <queue>

namespace dream::render {
namespace {
struct Point {
    double x, y;
};
struct Box {
    double x0, y0, x1, y1;
};
struct Triangle {
    std::array<Point, 3> points;
    Box box;
    double dx, dy, constant, orientation, tolerance;
};
struct Surface {
    std::vector<Triangle> triangles;
    Box box;
};
double cross(Point a, Point b, Point c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}
bool overlap(Box a, Box b) {
    return std::max(a.x0, b.x0) < std::min(a.x1, b.x1) &&
           std::max(a.y0, b.y0) < std::min(a.y1, b.y1);
}
Surface surface(const Frame& frame, const Polygon& p) {
    Surface s;
    s.box = {INFINITY, INFINITY, -INFINITY, -INFINITY};
    for (std::uint32_t k = 2; k < p.count; ++k) {
        const auto& a = frame.vertices[p.first + k - 2];
        const auto& b = frame.vertices[p.first + k - 1];
        const auto& c = frame.vertices[p.first + k];
        if (!(std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z) &&
              std::isfinite(b.x) && std::isfinite(b.y) && std::isfinite(b.z) &&
              std::isfinite(c.x) && std::isfinite(c.y) && std::isfinite(c.z)))
            continue;
        Triangle t;
        t.points = {Point{a.x, a.y}, Point{b.x, b.y}, Point{c.x, c.y}};
        const double area = cross(t.points[0], t.points[1], t.points[2]);
        if (std::abs(area) < 1e-8)
            continue;
        t.orientation = area > 0 ? 1 : -1;
        const double x1 = double(b.x) - a.x, y1 = double(b.y) - a.y, z1 = double(b.z) - a.z;
        const double x2 = double(c.x) - a.x, y2 = double(c.y) - a.y, z2 = double(c.z) - a.z;
        t.dx = (z1 * y2 - z2 * y1) / area;
        t.dy = (x1 * z2 - x2 * z1) / area;
        t.constant = a.z - t.dx * a.x - t.dy * a.y;
        t.tolerance = 1e-6 * std::max({1e-3, double(std::abs(a.z)), double(std::abs(b.z)),
                                       double(std::abs(c.z))});
        t.box = {std::min({double(a.x), double(b.x), double(c.x)}),
                 std::min({double(a.y), double(b.y), double(c.y)}),
                 std::max({double(a.x), double(b.x), double(c.x)}),
                 std::max({double(a.y), double(b.y), double(c.y)})};
        s.box = {std::min(s.box.x0, t.box.x0), std::min(s.box.y0, t.box.y0),
                 std::max(s.box.x1, t.box.x1), std::max(s.box.y1, t.box.y1)};
        s.triangles.push_back(t);
    }
    return s;
}
// 1: A is in front wherever the surfaces overlap; -1: B is in front;
// 0: no shared area, effectively coplanar, or genuinely intersecting surfaces.
int relation(const Surface& a, const Surface& b) {
    if (!overlap(a.box, b.box))
        return 0;
    bool front = false, back = false;
    for (const auto& ta : a.triangles)
        for (const auto& tb : b.triangles) {
            if (!overlap(ta.box, tb.box))
                continue;
            std::array<Point, 12> poly{}, next{};
            std::copy(ta.points.begin(), ta.points.end(), poly.begin());
            std::size_t count = 3;
            for (unsigned edge = 0; edge < 3 && count; ++edge) {
                const auto v0 = tb.points[edge], v1 = tb.points[(edge + 1) % 3];
                std::size_t added = 0;
                Point previous = poly[count - 1];
                double previous_side = tb.orientation * cross(v0, v1, previous);
                for (std::size_t k = 0; k < count; ++k) {
                    const Point current = poly[k];
                    const double side = tb.orientation * cross(v0, v1, current);
                    if ((side >= 0) != (previous_side >= 0)) {
                        const double t = previous_side / (previous_side - side);
                        next[added++] = {previous.x + (current.x - previous.x) * t,
                                         previous.y + (current.y - previous.y) * t};
                    }
                    if (side >= 0)
                        next[added++] = current;
                    previous = current;
                    previous_side = side;
                }
                poly = next;
                count = added;
            }
            if (count < 3)
                continue;
            double area = 0;
            for (std::size_t k = 1; k + 1 < count; ++k)
                area += cross(poly[0], poly[k], poly[k + 1]);
            if (std::abs(area) < 1e-5)
                continue;
            const double tolerance = std::max(ta.tolerance, tb.tolerance);
            for (std::size_t k = 0; k < count; ++k) {
                const double depth = (ta.dx - tb.dx) * poly[k].x + (ta.dy - tb.dy) * poly[k].y +
                                     ta.constant - tb.constant;
                front |= depth > tolerance;
                back |= depth < -tolerance;
            }
            if (front && back)
                return 0;
        }
    return front ? 1 : back ? -1 : 0;
}
} // namespace

void order_translucent_overlaps(const Frame& frame, std::vector<const Polygon*>& polygons) {
    const auto count = polygons.size();
    if (count < 2)
        return;
    std::vector<Surface> surfaces;
    surfaces.reserve(count);
    for (const auto* p : polygons)
        surfaces.push_back(surface(frame, *p));
    std::vector<std::vector<std::size_t>> edges(count);
    std::vector<unsigned> degree(count, 0);
    for (std::size_t i = 0; i < count; ++i)
        for (std::size_t j = i + 1; j < count; ++j) {
            const int order = relation(surfaces[i], surfaces[j]);
            if (!order)
                continue;
            const auto before = order > 0 ? j : i, after = order > 0 ? i : j;
            edges[before].push_back(after);
            ++degree[after];
        }
    std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>> ready;
    for (std::size_t i = 0; i < count; ++i)
        if (!degree[i])
            ready.push(i);
    std::vector<const Polygon*> result;
    std::vector<bool> emitted(count, false);
    result.reserve(count);
    while (result.size() < count) {
        // A cyclic visibility relation cannot be resolved by whole-strip draws.
        // Break ties using the existing order, rather than hanging or dropping faces.
        if (ready.empty()) {
            for (std::size_t i = 0; i < count; ++i)
                if (!emitted[i]) {
                    ready.push(i);
                    break;
                }
        }
        const auto index = ready.top();
        ready.pop();
        if (emitted[index])
            continue;
        emitted[index] = true;
        result.push_back(polygons[index]);
        for (auto after : edges[index])
            if (degree[after] && --degree[after] == 0 && !emitted[after])
                ready.push(after);
    }
    polygons = std::move(result);
}
} // namespace dream::render
