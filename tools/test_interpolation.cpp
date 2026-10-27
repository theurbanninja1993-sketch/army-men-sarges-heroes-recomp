// Standalone engine regression: compile together with engine/render/src/interpolate.cpp.
#include "dream/render/interpolate.h"
#undef NDEBUG
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace dream::render;

Frame scene(bool flat = false) {
    Frame f;
    f.vertices = {{100,100,1}, {200,100,1}, {100,200,flat ? 1.f : 2.f},
                  {200,100,1}, {200,200,flat ? 1.f : 2.f}, {100,200,flat ? 1.f : 2.f}};
    Polygon a; a.count = 3; a.tcw = 1;
    Polygon b = a; b.first = 3; b.tcw = 2;
    f.lists[0] = {a,b};
    return f;
}
int main() {
    BlendOptions options;
    std::vector<StripPair> pairs;
    BlendStats stats;
    Frame result;
    // A newly visible triangle must follow its neighbour's duplicated corners.
    auto newer = scene(), older = newer;
    older.lists[0].erase(older.lists[0].begin());
    for (auto& v : older.vertices) v.x -= 10;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(result.vertices[1].x == result.vertices[3].x);
    assert(result.vertices[2].x == result.vertices[5].x);
    assert(result.vertices[1].x == 195);
    // Shared snow corners differ by one ULP after projection. Include points
    // across a quantization boundary, and verify actual movement stays welded.
    newer = scene();
    newer.vertices[3].y = std::nextafter(newer.vertices[1].y, 0.f);
    newer.vertices[5].z = std::nextafter(newer.vertices[2].z, 3.f);
    older = newer;
    older.lists[0].erase(older.lists[0].begin());
    for (auto& v : older.vertices) v.x -= 10;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(result.vertices[1].x == 195);
    assert(result.vertices[1].x == result.vertices[3].x);
    assert(result.vertices[1].y == result.vertices[3].y);
    assert(result.vertices[2].z == result.vertices[5].z);
    // Similar projected corners on separate depth layers must stay separate.
    newer = scene();
    for (unsigned i=3;i<6;++i) newer.vertices[i].z *= 1.001f;
    older = newer;
    older.lists[0].erase(older.lists[0].begin());
    for (auto& v : older.vertices) v.x -= 10;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(result.vertices[1].x == 200 && result.vertices[3].x == 195);
    newer = scene();
    // Disagreeing strip matches retain one native corner instead of opening a gap.
    older = newer;
    for (unsigned i=0;i<3;++i) older.vertices[i].x -= 10;
    for (unsigned i=3;i<6;++i) older.vertices[i].x += 10;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(result.vertices[1].x == 200 && result.vertices[3].x == 200);
    assert(result.vertices[2].x == 100 && result.vertices[5].x == 100);
    // Coincident flat UI sprites remain independent.
    newer = scene(true); older = newer;
    for (unsigned i=0;i<3;++i) older.vertices[i].x -= 10;
    for (unsigned i=3;i<6;++i) older.vertices[i].x += 10;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(result.vertices[1].x == 195 && result.vertices[3].x == 205);
    // An unmatched planar wall must not bend when just one shared corner follows
    // a matched neighbour: that can put an exterior wall through its lining.
    newer = {};
    newer.vertices = {{100,100,1}, {200,100,1.5f}, {100,200,1}, {200,200,1.5f},
                      {100,100,1}, {50,50,.8f}, {40,150,.9f}};
    Polygon wall; wall.count=4; wall.tcw=10;
    Polygon neighbour; neighbour.first=4; neighbour.count=3; neighbour.tcw=11;
    newer.lists[0]={wall,neighbour}; older=newer;
    older.lists[0].erase(older.lists[0].begin());
    for(auto& v:older.vertices) v.x-=10;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    for(unsigned i=0;i<4;++i) {
        assert(result.vertices[i].x==newer.vertices[i].x);
        assert(result.vertices[i].z==newer.vertices[i].z);
    }
    assert(result.vertices[0].x==result.vertices[4].x); // repair must not open a seam
    // Natively nonplanar terrain strips must not be flattened or frozen.
    newer.vertices[3].z=2;
    older=newer; older.lists[0].erase(older.lists[0].begin());
    for(auto& v:older.vertices) v.x-=10;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(result.vertices[0].x==95 && result.vertices[4].x==95);
    newer.vertices[3].z=1.5f;
    // A fully matched planar panel can still interpolate normally.
    older=newer;
    for(auto& v:older.vertices) v.x-=10;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(result.vertices[0].x==95 && result.vertices[3].x==195);
    newer=scene(true);
    // Two individually planar wall layers must keep their depth order when
    // one layer interpolates and its nearby lining cannot be matched.
    newer = {};
    newer.vertices={{100,100,1},{200,100,1.5f},{100,200,1},{200,200,1.5f}};
    for(unsigned i=0;i<4;++i) { auto v=newer.vertices[i];v.z*=.99f;newer.vertices.push_back(v); }
    Polygon front;front.count=4;front.tcw=20;
    Polygon back=front;back.first=4;back.tcw=21;
    newer.lists[0]={front,back};
    for(bool interpolate_front:{true,false}) {
        older=newer;older.lists[0].erase(older.lists[0].begin()+(interpolate_front?1:0));
        for(auto& v:older.vertices) v.x+=interpolate_front?20.f:-20.f;
        assert(blend_frames(older,newer,options,pairs,result,stats));
        for(unsigned i=0;i<8;++i) assert(result.vertices[i].x==newer.vertices[i].x);
    }
    // Safe coherent movement of both layers stays interpolated.
    older=newer;for(auto& v:older.vertices) v.x+=20;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(result.vertices[0].x==110 && result.vertices[4].x==110);
    newer=scene(true);
    // Camera cuts keep the untouched native frame.
    older = newer;
    for (auto& v : older.vertices) v.x -= 100;
    assert(!blend_frames(older,newer,options,pairs,result,stats));
    assert(stats.cut && result.vertices[0].x == newer.vertices[0].x);

    // Shared material is insufficient: another texture/atlas corner must not
    // make an unrelated face a geometry interpolation partner.
    newer = scene();
    newer.lists[0].resize(1);
    newer.lists[0][0].pcw = 8;
    newer.vertices[1].u = 1;
    newer.vertices[2].v = 1;
    older = newer;
    for (auto& v : older.vertices) v.x -= 10;
    older.vertices[0].u = 0.25f;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(pairs.empty() && stats.blended_strips == 0);
    assert(result.vertices[0].x == newer.vertices[0].x);

    // The lookahead must skip an inserted face with the same material and
    // recover the actual face, leaving the unrelated new geometry in place.
    older = newer;
    for (auto& v : newer.vertices) v.x += 10;
    const auto first = static_cast<unsigned>(newer.vertices.size());
    for (unsigned i = 0; i < 3; ++i) {
        auto v = newer.vertices[i];
        v.x += 40; v.y += 30; v.u += 0.25f;
        newer.vertices.push_back(v);
    }
    auto extra = newer.lists[0][0]; extra.first = first;
    newer.lists[0].insert(newer.lists[0].begin(), extra);
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(pairs.size() == 1 && pairs[0].a == 0 && pairs[0].b == 1);
    assert(result.vertices[0].x == 105 && result.vertices[first].x == 150);

    // Repeated faces can also share UVs: opposite winding or a drastic area
    // change must not collapse/flip a tree branch between native frames.
    newer = older;
    std::swap(older.vertices[1].x, older.vertices[2].x);
    std::swap(older.vertices[1].y, older.vertices[2].y);
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(pairs.empty() && result.vertices[1].x == newer.vertices[1].x);
    older = newer;
    older.vertices[1].x = 110;
    older.vertices[2].y = 110;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(pairs.empty() && result.vertices[1].x == newer.vertices[1].x);
    // Visibility can remove more faces than the ordinal lookahead covers. A
    // repeated texture at another depth must not steal the surviving face.
    newer = scene(); newer.lists[0].resize(1);
    older = newer;
    for (auto& v : older.vertices) v.x -= 10;
    const auto actual = older.lists[0][0];
    for (unsigned n = 0; n < 12; ++n) {
        auto extra = actual;
        extra.tcw = 100 + n;
        older.lists[0].insert(older.lists[0].begin(), extra);
    }
    auto impostor = actual;
    impostor.first = static_cast<unsigned>(older.vertices.size());
    for (unsigned k = 0; k < 3; ++k) {
        auto v = newer.vertices[k];
        v.z *= 0.65f;
        older.vertices.push_back(v);
    }
    older.lists[0].insert(older.lists[0].begin(), impostor);
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(pairs.size() == 1 && pairs[0].a == 13);
    assert(result.vertices[0].x == 95);

    // Even a compatible ordinal pair must yield to a nearer copy of the same
    // face. Repeated tree/log geometry otherwise blends into its neighbour.
    older = newer;
    for (auto& v : older.vertices) v.x -= 40;
    auto near = older.lists[0][0];
    near.first = static_cast<unsigned>(older.vertices.size());
    for (unsigned k = 0; k < 3; ++k) {
        auto v = newer.vertices[k]; v.x -= 10;
        older.vertices.push_back(v);
    }
    older.lists[0].push_back(near);
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(pairs.size() == 1 && pairs[0].a == 1);
    assert(result.vertices[0].x == 95);

    // Equal endpoint winding/area can conceal a reversed corner assignment:
    // rotating all corners by 180 degrees collapses the intermediate triangle.
    older = newer;
    for (unsigned k = 0; k < 3; ++k) {
        older.vertices[k].x = 200 - newer.vertices[k].x;
        older.vertices[k].y = 200 - newer.vertices[k].y;
    }
    assert(blend_frames(older,newer,options,pairs,result,stats));
    assert(pairs.empty());

    // A new skinny face can fold after inheriting only two moving corners.
    // Restore all shared copies and recheck neighbours, without splitting seams.
    newer = scene();
    newer.vertices = {{0,0,1},{20,0,1},{0,100,2},
                      {20,0,1},{30,100,2},{0,100,2}};
    older = newer;
    older.lists[0].resize(1);
    for (auto& v : older.vertices) v.x += 60;
    options.cut_median = 80;
    assert(blend_frames(older,newer,options,pairs,result,stats));
    options.cut_median = 40;
    assert(result.vertices[1].x == 20 && result.vertices[3].x == 20);
    assert(result.vertices[2].x == 0 && result.vertices[5].x == 0);

    // A camera translation must keep a sloping planar quad planar. Check the
    // actual projected midpoint, rather than linearly averaging screen pixels.
    newer = {}; older = {};
    Polygon quad; quad.count = 4;
    newer.lists[0] = older.lists[0] = {quad};
    for (unsigned k = 0; k < 4; ++k) {
        const float x = float(k & 1), y = float(k >> 1), z = 10 + 2*x;
        older.vertices.push_back({320+100*x/z,240+100*y/z,1/z});
        newer.vertices.push_back({320+100*(x+.1f)/(z+.5f),240+100*y/(z+.5f),1/(z+.5f)});
    }
    for (float t : {0.f, .25f, .5f, 1.f}) {
        options.t = t;
        assert(blend_frames(older,newer,options,pairs,result,stats));
        assert(stats.blended_strips == 1);
        for (unsigned k = 0; k < 4; ++k) {
            const float x = float(k & 1), y = float(k >> 1), z = 10+2*x+.5f*t;
            assert(std::abs(result.vertices[k].x - (320+100*(x+.1f*t)/z)) < .0001f);
            assert(std::abs(result.vertices[k].y - (240+100*y/z)) < .0001f);
            assert(std::abs(result.vertices[k].z - 1/z) < .000001f);
        }
    }
    std::puts("interpolation shared-edge, planar-panel, wall-layer, correspondence and projection regressions passed");
}
