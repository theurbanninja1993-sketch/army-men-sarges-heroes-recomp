#include "dream/render/interpolate.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <map>
#include <array>

namespace dream::render {

namespace {

bool same(const Frame &older, const Polygon &a, const Frame &newer, const Polygon &b) {
    if (a.count != b.count || a.pcw != b.pcw || a.isp != b.isp || a.tsp != b.tsp ||
        a.tcw != b.tcw || a.tsp1 != b.tsp1 || a.tcw1 != b.tcw1)
        return false;

    // Many unrelated faces share a material. Matching only render state can
    // pair different tree branches, terrain faces, or atlas regions after culling
    // changes submission order. Require the same texture corner correspondence.
    if (a.pcw & 8u) {
        for (std::uint32_t k = 0; k < a.count; ++k) {
            const Vertex &va = older.vertices[a.first + k];
            const Vertex &vb = newer.vertices[b.first + k];
            if (!std::isfinite(va.u) || !std::isfinite(va.v) || !std::isfinite(vb.u) ||
                !std::isfinite(vb.v) || std::abs(va.u - vb.u) > 0.00001f ||
                std::abs(va.v - vb.v) > 0.00001f)
                return false;
        }
    }

    auto area = [](const Vertex &v0, const Vertex &v1, const Vertex &v2) {
        return (v1.x - v0.x) * (v2.y - v0.y) - (v1.y - v0.y) * (v2.x - v0.x);
    };
    for (std::uint32_t k = 2; k < a.count; ++k) {
        const float aa = area(older.vertices[a.first + k - 2], older.vertices[a.first + k - 1],
                              older.vertices[a.first + k]);
        const float ab = area(newer.vertices[b.first + k - 2], newer.vertices[b.first + k - 1],
                              newer.vertices[b.first + k]);
        if (!std::isfinite(aa) || !std::isfinite(ab))
            return false;
        const float small = std::min(std::abs(aa), std::abs(ab));
        const float large = std::max(std::abs(aa), std::abs(ab));
        // Similar UVs are still ambiguous on repeated faces. Opposite winding
        // or a drastic area change is not a safe correspondence; keep the native
        // face when it turns edge-on or its clipped topology changes instead.
        if (small > 1.0f && (aa * ab < 0.0f || small < large * 0.25f))
            return false;
        Vertex mid[3];
        for (unsigned n = 0; n < 3; ++n) {
            const auto &va = older.vertices[a.first + k - 2 + n];
            const auto &vb = newer.vertices[b.first + k - 2 + n];
            mid[n].x = (va.x + vb.x) * 0.5f;
            mid[n].y = (va.y + vb.y) * 0.5f;
        }
        const float am = area(mid[0], mid[1], mid[2]);
        if (small > 1.0f && (am * ab <= 0 || std::abs(am) < small * 0.25f))
            return false;
    }
    return true;
}

// Repeated materials and UVs need not retain submission order after culling.
// Choose the closest compatible face across the list, with unique old partners.
// Relative depth is part of the score so a nearby tree at another distance
// cannot become a partner merely because it uses the same foliage atlas.
MatchStats match_spatial(const Frame &older, const Frame &newer, std::vector<StripPair> &out) {
    out.clear();
    MatchStats st;
    for (std::uint32_t l = 0; l < newer.lists.size(); ++l) {
        const auto &A = older.lists[l];
        const auto &B = newer.lists[l];
        st.strips_a += static_cast<std::uint32_t>(A.size());
        st.strips_b += static_cast<std::uint32_t>(B.size());
        for (const auto &p : B)
            st.vertices_b += p.count;
        std::map<std::array<std::uint32_t, 8>, std::vector<std::uint32_t>> buckets;
        auto key = [](const Polygon &p) {
            return std::array<std::uint32_t, 8>{p.count, p.pcw,  p.isp,  p.tsp,
                                                p.tcw,   p.tsp1, p.tcw1, p.tile_clip};
        };
        for (std::uint32_t i = 0; i < A.size(); ++i)
            buckets[key(A[i])].push_back(i);
        struct Candidate {
            float cost;
            std::uint32_t a, b;
        };
        std::vector<Candidate> candidates;
        for (std::uint32_t j = 0; j < B.size(); ++j) {
            auto found = buckets.find(key(B[j]));
            if (found == buckets.end())
                continue;
            float best = INFINITY;
            std::uint32_t chosen = 0;
            for (auto i : found->second) {
                const auto &first_a = older.vertices[A[i].first];
                const auto &first_b = newer.vertices[B[j].first];
                const float first_scale = std::max(std::abs(first_a.z), std::abs(first_b.z));
                const float relative_z = first_scale > 0 ? (first_b.z - first_a.z) / first_scale : 0;
                if (std::abs(relative_z) > .1f)
                    continue;
                const float first_dx = first_b.x - first_a.x, first_dy = first_b.y - first_a.y;
                if (first_dx * first_dx + first_dy * first_dy + 320.f * 320.f * relative_z * relative_z >= best)
                    continue;
                if (!same(older, A[i], newer, B[j]))
                    continue;
                float cost = 0;
                bool ok = true;
                for (std::uint32_t k = 0; k < B[j].count; ++k) {
                    const auto &va = older.vertices[A[i].first + k];
                    const auto &vb = newer.vertices[B[j].first + k];
                    const float dx = vb.x - va.x, dy = vb.y - va.y;
                    const float scale = std::max(std::abs(va.z), std::abs(vb.z));
                    const float dz = scale > 0 ? (vb.z - va.z) / scale : 0;
                    if (std::abs(dz) > .1f) {
                        ok = false;
                        break;
                    }
                    cost += dx * dx + dy * dy + 320.f * 320.f * dz * dz;
                }
                if (ok && cost < best) {
                    best = cost;
                    chosen = i;
                }
            }
            if (std::isfinite(best))
                candidates.push_back({best, chosen, j});
        }
        std::stable_sort(candidates.begin(), candidates.end(),
                         [](const auto &a, const auto &b) { return a.cost < b.cost; });
        std::vector<bool> used(A.size(), false);
        for (const auto &c : candidates) {
            if (used[c.a])
                continue;
            used[c.a] = true;
            out.push_back({l, c.a, c.b});
            ++st.matched;
            st.matched_vertices += B[c.b].count;
        }
    }
    return st;
}

} // namespace

MatchStats match_frames(const Frame &older, const Frame &newer, unsigned window,
                        std::vector<StripPair> &out) {
    out.clear();
    MatchStats st;
    for (std::uint32_t l = 0; l < newer.lists.size(); ++l) {
        const auto &A = older.lists[l];
        const auto &B = newer.lists[l];
        st.strips_a += static_cast<std::uint32_t>(A.size());
        st.strips_b += static_cast<std::uint32_t>(B.size());
        for (const auto &p : B)
            st.vertices_b += p.count;
        std::size_t i = 0, j = 0;
        while (i < A.size() && j < B.size()) {
            if (same(older, A[i], newer, B[j])) {
                out.push_back({l, static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(j)});
                ++st.matched;
                st.matched_vertices += B[j].count;
                ++i;
                ++j;
                continue;
            }
            // The nearest resynchronisation: older has extra strips, or newer has new ones.
            std::size_t skip_a = 0, skip_b = 0;
            for (unsigned d = 1; d <= window; ++d) {
                if (i + d < A.size() && same(older, A[i + d], newer, B[j])) {
                    skip_a = d;
                    break;
                }
                if (j + d < B.size() && same(older, A[i], newer, B[j + d])) {
                    skip_b = d;
                    break;
                }
            }
            if (skip_a)
                i += skip_a;
            else if (skip_b)
                j += skip_b;
            else {
                ++i;
                ++j;
            }
        }
    }
    return st;
}

bool blend_frames(const Frame &older, const Frame &newer, const BlendOptions &opt,
                  std::vector<StripPair> &pairs, Frame &out, BlendStats &stats) {
    out = newer;
    stats = BlendStats{};
    stats.match = match_frames(older, newer, opt.window, pairs);
    // Camera cut test: the median movement of paired, on-screen first vertices.
    std::vector<float> motion;
    motion.reserve(pairs.size());
    for (const auto &p : pairs) {
        const Vertex &va = older.vertices[older.lists[p.list][p.a].first];
        const Vertex &vb = newer.vertices[newer.lists[p.list][p.b].first];
        if (vb.x >= 0.0f && vb.x <= 640.0f && vb.y >= 0.0f && vb.y <= 480.0f)
            motion.push_back(std::hypot(vb.x - va.x, vb.y - va.y));
    }
    if (!motion.empty()) {
        std::nth_element(motion.begin(), motion.begin() + motion.size() / 2, motion.end());
        stats.median_motion = motion[motion.size() / 2];
    }
    if (stats.median_motion > opt.cut_median) {
        stats.cut = true;
        return false;
    }
    // Keep the ordered probe above for cuts; refine real scene correspondence
    // only after establishing continuity.
    stats.match = match_spatial(older, newer, pairs);
    std::vector<bool> blended(out.vertices.size(), false);
    const float t = opt.t;
    for (const auto &p : pairs) {
        const Polygon &pa = older.lists[p.list][p.a];
        const Polygon &pb = out.lists[p.list][p.b];
        bool ok = true;
        for (std::uint32_t k = 0; k < pb.count && ok; ++k) {
            const Vertex &va = older.vertices[pa.first + k];
            const Vertex &vb = out.vertices[pb.first + k];
            ok = std::isfinite(va.x) && std::isfinite(va.y) && std::isfinite(va.z) &&
                 std::isfinite(vb.x) && std::isfinite(vb.y) && std::isfinite(vb.z) && va.z > 0 &&
                 vb.z > 0 && std::abs(vb.x - va.x) <= opt.max_jump &&
                 std::abs(vb.y - va.y) <= opt.max_jump;
        }
        if (!ok) {
            ++stats.jumped_strips;
            continue;
        }
        for (std::uint32_t k = 0; k < pb.count; ++k) {
            const Vertex &va = older.vertices[pa.first + k];
            Vertex &vb = out.vertices[pb.first + k];
            blended[pb.first + k] = true;
            // Interpolate pre-projection positions, then project again. Linear
            // screen-space interpolation bends coplanar surfaces with different
            // tessellations, so overlays can alternately pass/fail the depth test.
            const double denominator = (1.0 - t) * vb.z + t * va.z;
            const float weight = static_cast<float>(t * va.z / denominator);
            vb.x = va.x + (vb.x - va.x) * weight;
            vb.y = va.y + (vb.y - va.y) * weight;
            vb.z = va.z + (vb.z - va.z) * weight;
        }
        ++stats.blended_strips;
    }
    // Terrain edges often duplicate their vertices in separately submitted strips.
    // A strip missing from the older frame must follow its matched neighbours at
    // shared corners. Conflicting matches keep that corner at the native position.
    // Flat sprites/HUD layers are independent even when their positions coincide.
    std::vector<bool> used(newer.vertices.size(), false);
    std::vector<bool> spatial(newer.vertices.size(), false);
    for (const auto &list : newer.lists) {
        for (const auto &poly : list) {
            if (poly.count < 3)
                continue;
            const float z = newer.vertices[poly.first].z;
            bool flat = true;
            for (std::uint32_t k = 1; k < poly.count; ++k)
                flat = flat && newer.vertices[poly.first + k].z == z;

            for (std::uint32_t k = 0; k < poly.count; ++k) {
                const auto index = poly.first + k;
                spatial[index] = spatial[index] || !flat;
                used[index] = true;
            }
        }
    }
    std::vector<std::size_t> parent(out.vertices.size());
    for (std::size_t i = 0; i < parent.size(); ++i)
        parent[i] = i;
    auto root = [&](std::size_t i) {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    };
    auto join = [&](std::size_t a, std::size_t b) { parent[root(b)] = root(a); };
    // Separate projections of the same world corner can differ by a few float
    // ULPs (notably on the snow terrain). Search neighbouring screen-space cells
    // and require near-identical reciprocal depth, so rounding cannot split an
    // edge and distinct surfaces at the same screen position stay independent.
    constexpr double cell_size = 0.001;
    using Cell = std::array<std::int64_t, 2>;
    struct CellHash {
        std::size_t operator()(const Cell &cell) const {
            auto h = static_cast<std::uint64_t>(cell[0]) * 0x9e3779b97f4a7c15ULL +
                     static_cast<std::uint64_t>(cell[1]);
            h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
            h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
            return static_cast<std::size_t>(h ^ (h >> 31));
        }
    };
    std::unordered_map<Cell, std::vector<std::size_t>, CellHash> cells;
    cells.reserve(newer.vertices.size());
    for (std::size_t i = 0; i < newer.vertices.size(); ++i) {
        const auto &v = newer.vertices[i];
        if (!used[i] || !std::isfinite(v.x) || !std::isfinite(v.y) ||
            !std::isfinite(v.z) || std::abs(v.x) > 1e9f || std::abs(v.y) > 1e9f)
            continue;
        const Cell cell{static_cast<std::int64_t>(std::floor(v.x / cell_size)),
                        static_cast<std::int64_t>(std::floor(v.y / cell_size))};
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy) {
                const auto found = cells.find({cell[0] + dx, cell[1] + dy});
                if (found == cells.end())
                    continue;
                for (auto j : found->second) {
                    const auto &w = newer.vertices[j];
                    if ((spatial[i] || spatial[j]) &&
                        std::abs(double(v.x) - w.x) <= cell_size &&
                        std::abs(double(v.y) - w.y) <= cell_size &&
                        std::abs(v.z - w.z) <=
                            std::max(1e-7f, std::max(std::abs(v.z), std::abs(w.z)) * 1e-6f))
                        join(i, j);
                }
            }
        cells[cell].push_back(i);
    }
    std::map<std::size_t, std::vector<std::size_t>> corners;
    for (std::size_t i = 0; i < newer.vertices.size(); ++i)
        if (used[i])
            corners[root(i)].push_back(i);
    for (const auto &entry : corners) {
        const auto &indices = entry.second;
        if (indices.size() < 2)
            continue;
        if (std::none_of(indices.begin(), indices.end(), [&](auto i) { return spatial[i]; }))
            continue;
        for (auto i : indices)
            join(indices.front(), i);
        const Vertex *candidate = nullptr;
        bool conflict = false;
        for (auto i : indices) {
            if (!blended[i])
                continue;
            const auto &v = out.vertices[i];
            if (!candidate)
                candidate = &v;
            else if (std::abs(v.x - candidate->x) > 0.001f ||
                     std::abs(v.y - candidate->y) > 0.001f ||
                     std::abs(v.z - candidate->z) >
                         std::max(0.000001f, std::abs(candidate->z) * 0.00001f))
                conflict = true;
        }
        if (!candidate)
            continue;
        const Vertex position = conflict ? newer.vertices[indices.front()] : *candidate;
        for (auto i : indices) {
            out.vertices[i].x = position.x;
            out.vertices[i].y = position.y;
            out.vertices[i].z = position.z;
        }
    }
    // Partial matches can fold triangles or bend a planar strip (for example an
    // exterior wall into its interior lining). Restore unsafe surfaces and every
    // shared copy together, then recheck their neighbours until stable.
    struct Constraint {
        std::size_t first, count, corner_first, basis;
        std::size_t layer = std::size_t(-1);
    };
    std::vector<Constraint> constraints;
    std::vector<std::size_t> constraint_corners;
    std::vector<std::vector<std::size_t>> incident(out.vertices.size()),
        members(out.vertices.size());
    for (std::size_t i = 0; i < out.vertices.size(); ++i)
        members[root(i)].push_back(i);
    auto area = [](const Vertex &a, const Vertex &b, const Vertex &c) {
        return (double(b.x) - a.x) * (double(c.y) - a.y) -
               (double(b.y) - a.y) * (double(c.x) - a.x);
    };
    struct DepthPlane { double x, y, constant; bool valid; };
    auto depth_plane = [&](const Frame &f, std::size_t first, std::size_t basis) {
        const auto &a = f.vertices[first + basis];
        const auto &b = f.vertices[first + basis + 1];
        const auto &c = f.vertices[first + basis + 2];
        const double det = area(a, b, c);
        if (!std::isfinite(det) || std::abs(det) < 1.0)
            return DepthPlane{0, 0, 0, false};
        const double zx = ((double(b.z) - a.z) * (double(c.y) - a.y) -
                           (double(c.z) - a.z) * (double(b.y) - a.y)) / det;
        const double zy = ((double(b.x) - a.x) * (double(c.z) - a.z) -
                           (double(c.x) - a.x) * (double(b.z) - a.z)) / det;
        return DepthPlane{zx, zy, a.z - zx * a.x - zy * a.y, true};
    };
    auto depth_at = [](const DepthPlane &p, double x, double y) {
        return p.x * x + p.y * y + p.constant;
    };
    auto planar = [&](const Frame &f, std::size_t first, std::size_t count,
                      std::size_t basis, double tolerance) {
        const auto plane = depth_plane(f, first, basis);
        if (!plane.valid) return false;
        for (std::size_t k = 0; k < count; ++k) {
            const auto &v = f.vertices[first + k];
            const double z = depth_at(plane, v.x, v.y);
            if (!std::isfinite(z) || std::abs(z - v.z) >
                std::max(1e-7, std::abs(double(v.z)) * tolerance))
                return false;
        }
        return true;
    };
    struct Panel {
        std::size_t first, count, basis;
        DepthPlane plane;
        std::array<double, 3> normal;
        double x0, y0, x1, y1;
    };
    std::vector<Panel> panels;
    struct Layer {
        std::size_t a, b;
        double x0, y0, x1, y1;
        int order;
    };
    std::vector<Layer> layers;
    auto add_constraint = [&](std::size_t first, std::size_t count, std::size_t basis) {
        const auto index = constraints.size();
        constraints.push_back({first, count, constraint_corners.size(), basis});
        for (std::size_t k = 0; k < count; ++k) {
            const auto corner = root(first + k);
            constraint_corners.push_back(corner);
            incident[corner].push_back(index);
        }
    };
    for (std::size_t l = 0; l < newer.lists.size(); ++l)
        for (const auto &poly : newer.lists[l]) {
            double largest = 0;
            std::size_t basis = 0;
            for (std::uint32_t k = 2; k < poly.count; ++k) {
                const std::size_t first = poly.first + k - 2;
                add_constraint(first, 3, 0);
                const double size = std::abs(area(newer.vertices[first], newer.vertices[first + 1],
                                                  newer.vertices[first + 2]));
                if (size > largest) { largest = size; basis = k - 2; }
            }
            // Flat HUD layers and natively nonplanar strips retain their existing rules.
            if (largest < 1 || !spatial[poly.first] ||
                !planar(newer, poly.first, poly.count, basis, 1e-5)) continue;
            if (poly.count > 3) add_constraint(poly.first, poly.count, basis);
            if (l != 0 && l != 4) continue; // depth-writing world surfaces
            Panel panel{poly.first, poly.count, basis, depth_plane(newer, poly.first, basis),
                        {}, 640, 480, 0, 0};
            for (std::size_t k = 0; k < poly.count; ++k) {
                const auto &v = newer.vertices[poly.first+k];
                panel.x0 = std::min(panel.x0, double(v.x)); panel.x1 = std::max(panel.x1, double(v.x));
                panel.y0 = std::min(panel.y0, double(v.y)); panel.y1 = std::max(panel.y1, double(v.y));
            }
            panel.x0 = std::max(0., panel.x0); panel.x1 = std::min(640., panel.x1);
            panel.y0 = std::max(0., panel.y0); panel.y1 = std::min(480., panel.y1);
            if (panel.x1-panel.x0 < 1 || panel.y1-panel.y0 < 1) continue;
            // Compare plane orientations in centred, normalized screen coordinates.
            panel.normal = {320*panel.plane.x, 320*panel.plane.y,
                depth_at(panel.plane, 320, 240)};
            const double norm = std::hypot(panel.normal[0], panel.normal[1], panel.normal[2]);
            if (!(norm > 0) || !std::isfinite(norm)) continue;
            for (auto &v : panel.normal) v /= norm;
            panels.push_back(panel);
        }
    // A panel can stay planar yet cross a nearby parallel lining when only one
    // layer interpolates. Preserve the native ordering of such thin wall layers.
    // A screen grid limits comparisons, and coplanar neighbours are unconstrained.
    std::array<std::vector<std::size_t>, 80> grid;
    std::vector<std::size_t> seen(panels.size(), std::size_t(-1));
    for (std::size_t i = 0; i < panels.size(); ++i) {
        const auto &a = panels[i];
        const int x0 = std::clamp(int(a.x0 / 64), 0, 9), x1 = std::clamp(int(a.x1 / 64), 0, 9);
        const int y0 = std::clamp(int(a.y0 / 64), 0, 7), y1 = std::clamp(int(a.y1 / 64), 0, 7);
        for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
            auto &cell = grid[std::size_t(y * 10 + x)];
            for (auto j : cell) {
                if (seen[j] == i) continue;
                seen[j] = i;
                const auto &b = panels[j];
                const double dot = a.normal[0]*b.normal[0]+a.normal[1]*b.normal[1]+a.normal[2]*b.normal[2];
                if (dot < 1-1e-7) continue;
                Layer layer{i, j, std::max(a.x0,b.x0), std::max(a.y0,b.y0),
                            std::min(a.x1,b.x1), std::min(a.y1,b.y1), 0};
                if (layer.x1-layer.x0 < 1 || layer.y1-layer.y0 < 1) continue;
                bool front = false, back = false, close = true;
                for (double px : {layer.x0,layer.x1}) for (double py : {layer.y0,layer.y1}) {
                    const double za = depth_at(a.plane,px,py), zb = depth_at(b.plane,px,py);
                    const double scale = std::max(std::abs(za),std::abs(zb));
                    if (za <= 0 || zb <= 0) continue; // extrapolated area outside the visible panel
                    close &= std::abs(za-zb) < scale*.05;
                    front |= za-zb > scale*2e-6; back |= zb-za > scale*2e-6;
                }
                if (!close || front == back) continue;
                layer.order = front ? 1 : -1;
                const auto index = constraints.size(), corners_at = constraint_corners.size();
                for (const auto *panel : {&a,&b}) for (std::size_t k = 0; k < panel->count; ++k) {
                    const auto corner = root(panel->first+k);
                    constraint_corners.push_back(corner); incident[corner].push_back(index);
                }
                constraints.push_back({a.first,a.count+b.count,corners_at,0,layers.size()});
                layers.push_back(layer);
            }
            cell.push_back(i);
        }
    }
    std::vector<std::size_t> pending(constraints.size());
    for (std::size_t i = 0; i < pending.size(); ++i)
        pending[i] = i;
    std::vector<bool> queued(constraints.size(), true), restored(out.vertices.size(), false);
    for (std::size_t next = 0; next < pending.size(); ++next) {
        const auto index = pending[next];
        queued[index] = false;
        const auto &surface = constraints[index];
        const auto first = surface.first;
        if (surface.layer != std::size_t(-1)) {
            const auto &layer = layers[surface.layer];
            const auto &a = panels[layer.a], &b = panels[layer.b];
            const auto pa = depth_plane(out,a.first,a.basis), pb = depth_plane(out,b.first,b.basis);
            bool reversed = !pa.valid || !pb.valid;
            for (double px : {layer.x0,layer.x1}) for (double py : {layer.y0,layer.y1}) {
                const double za = depth_at(pa,px,py), zb = depth_at(pb,px,py);
                const double tolerance = std::max(1e-7,std::max(std::abs(za),std::abs(zb))*2e-6);
                reversed |= !std::isfinite(za) || !std::isfinite(zb) ||
                    (za > 0 && zb > 0 && (za-zb)*layer.order < -tolerance);
            }
            if (!reversed) continue;
        } else if (surface.count == 3) {
            const double native = area(newer.vertices[first], newer.vertices[first + 1], newer.vertices[first + 2]);
            const double mixed = area(out.vertices[first], out.vertices[first + 1], out.vertices[first + 2]);
            if (std::abs(native) <= 1 ||
                (native * mixed > 0 && std::abs(mixed) >= std::abs(native) * .25))
                continue;
        } else if (planar(out, first, surface.count, surface.basis, 1e-4))
            continue;
        for (std::size_t k = 0; k < surface.count; ++k) {
            const auto corner = constraint_corners[surface.corner_first + k];
            if (restored[corner])
                continue;
            restored[corner] = true;
            for (auto i : members[corner]) {
                out.vertices[i].x = newer.vertices[corner].x;
                out.vertices[i].y = newer.vertices[corner].y;
                out.vertices[i].z = newer.vertices[corner].z;
            }
            for (auto neighbor : incident[corner])
                if (!queued[neighbor]) {
                    queued[neighbor] = true;
                    pending.push_back(neighbor);
                }
        }
    }
    // Blended depths lie between the two frames': keep the projection range covering both.
    out.min_z = std::min(older.min_z, newer.min_z);
    out.max_z = std::max(older.max_z, newer.max_z);
    return true;
}

} // namespace dream::render
