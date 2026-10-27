#include "dream/render/translucent_sort.h"
#undef NDEBUG
#include <cassert>
#include <cstdio>
using namespace dream::render;
Polygon triangle(Frame& f, Vertex a, Vertex b, Vertex c) {
    Polygon p; p.first=static_cast<unsigned>(f.vertices.size());p.count=3;
    f.vertices.insert(f.vertices.end(),{a,b,c});return p;
}
int main() {
    Frame frame;
    // The broad triangle extends far away but is entirely in front at its
    // overlap with the small triangle. A minimum-depth scalar gives the wrong
    // order; using the overlap must restore back before front.
    auto front=triangle(frame,{0,0,.1f},{100,0,2},{100,100,2});
    auto back=triangle(frame,{80,10,1},{90,10,1},{90,30,1});
    std::vector<const Polygon*> order{&front,&back};
    order_translucent_overlaps(frame,order);
    assert(order[0]==&back && order[1]==&front);
    // Correct existing order is stable.
    order_translucent_overlaps(frame,order);
    assert(order[0]==&back && order[1]==&front);

    // Bounds can overlap without any shared pixels. Do not impose an order
    // between the two disjoint triangular regions of this square.
    frame={};
    front=triangle(frame,{0,0,2},{100,0,2},{0,100,2});
    back=triangle(frame,{100,100,1},{100,60,1},{60,100,1});
    order={&front,&back};order_translucent_overlaps(frame,order);
    assert(order[0]==&front && order[1]==&back);

    // Touching edges carry no area and must not shuffle adjacent faces.
    frame={};
    front=triangle(frame,{0,0,2},{100,0,2},{0,100,2});
    back=triangle(frame,{100,0,1},{100,100,1},{0,100,1});
    order={&front,&back};order_translucent_overlaps(frame,order);
    assert(order[0]==&front && order[1]==&back);

    // Coplanar overlays retain their original priority.
    frame={};front=triangle(frame,{0,0,1},{100,0,1},{0,100,1});
    back=triangle(frame,{0,0,1},{100,0,1},{0,100,1});
    order={&front,&back};order_translucent_overlaps(frame,order);
    assert(order[0]==&front && order[1]==&back);

    // Intersecting planes have no whole-face ordering. Retain the fallback.
    frame={};front=triangle(frame,{0,0,.5f},{100,0,1.5f},{0,100,.5f});
    back=triangle(frame,{0,0,1},{100,0,1},{0,100,1});
    order={&front,&back};order_translucent_overlaps(frame,order);
    assert(order[0]==&front && order[1]==&back);

    // Look at every triangle in a strip, including its alternating winding.
    // The small back face overlaps only the strip's second triangle.
    frame={};front.first=0;front.count=4;
    frame.vertices={{0,0,2},{100,0,2},{0,100,2},{100,100,2}};
    back=triangle(frame,{80,80,1},{90,80,1},{80,90,1});
    order={&front,&back};order_translucent_overlaps(frame,order);
    assert(order[0]==&back && order[1]==&front);

    // A chain of overlapping layers is ordered even when submitted backwards.
    auto far=triangle(frame,{80,80,.5f},{90,80,.5f},{80,90,.5f});
    order={&front,&back,&far};order_translucent_overlaps(frame,order);
    assert(order[0]==&far && order[1]==&back && order[2]==&front);
    order.clear();order_translucent_overlaps(frame,order);assert(order.empty());
    std::puts("translucent overlap ordering regressions passed");
}
