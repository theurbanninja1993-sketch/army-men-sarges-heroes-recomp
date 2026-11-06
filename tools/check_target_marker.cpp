// Capture-only HUD regression check. Inputs remain local; no game data in source.
#include "dream/render/background.h"
#include "dream/render/display_list.h"
#include "dream/render/texture.h"
#include "dream/render/png.h"
#include "dreamcomp/hud.h"
#include <fstream>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <vector>
template<class T> std::vector<T> read(const std::string& name) {
 std::ifstream f(name,std::ios::binary|std::ios::ate);if(!f)return {};
 std::vector<T> b(std::size_t(f.tellg())/sizeof(T));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),b.size()*sizeof(T));return b;
}
int main(int argc,char**argv) {
 if(argc<6){std::fprintf(stderr,"usage: check_target_marker FRAME.ta VRAM TCW OLD.ini NEW.ini [texture.png|--allow-missing]\n");return 2;}
 auto ta=read<std::uint32_t>(argv[1]);auto vram=read<std::uint8_t>(argv[2]);auto regs=read<std::uint32_t>(std::string(argv[2])+".regs");
 if(ta.empty()||vram.size()!=8388608||regs.size()!=2048)return 2;
 dream::render::DisplayList decoder;decoder.feed_stream(ta.data(),ta.size());auto original=decoder.frame();
 dream::render::add_background(original,regs.data(),vram.data(),vram.size());
 const bool allow_missing=argc>6 && std::string(argv[6])=="--allow-missing";
 const auto tcw=std::uint32_t(std::strtoul(argv[3],nullptr,0));
 std::vector<bool> marker(original.vertices.size());unsigned count=0;
 for(auto&list:original.lists)for(auto&p:list)if(p.tcw==tcw && (p.pcw&8)) {
  for(unsigned k=0;k<p.count;++k){marker[p.first+k]=true;++count;}
  if(argc>6&&!allow_missing){dream::render::TextureInfo info;std::vector<std::uint32_t> pixels;
   if(!dream::render::describe_texture(p.tcw,p.tsp,0,info)||!dream::render::decode_texture(info,vram.data(),vram.size(),nullptr,pixels))return 2;
   auto png=dream::render::png::encode(pixels.data(),info.width,info.height);
   std::ofstream f(argv[6],std::ios::binary);f.write(reinterpret_cast<const char*>(png.data()),png.size());
  }
 }
 if(!count&&!allow_missing){std::fprintf(stderr,"no marker polygons\n");return 2;}
 dreamcomp::hud::Overrides before,after;std::string error;
 if(!before.load(argv[4],&error)||!after.load(argv[5],&error)){std::fprintf(stderr,"%s\n",error.c_str());return 2;}
 for(float aspect:{4.f/3.f,16.f/10.f,16.f/9.f}) {
  auto a=original,b=original;dreamcomp::PortInfo::HudRule rule;rule.enabled=true;rule.polygons=true;dreamcomp::hud::Stats stats;
  dreamcomp::hud::correct(a,rule,aspect,true,true,before,nullptr,stats);
  dreamcomp::hud::correct(b,rule,aspect,true,true,after,nullptr,stats);
  float displacement=0;unsigned moved=0;
  for(std::size_t i=0;i<marker.size();++i){auto&src=original.vertices[i];auto&old=a.vertices[i];auto&now=b.vertices[i];
   if(marker[i]){displacement=std::max(displacement,std::abs(old.x-src.x));moved+=old.x!=src.x;
    if(now.x!=src.x||now.y!=src.y||now.z!=src.z){std::fprintf(stderr,"FAIL: marker changed\n");return 1;}}
   else if(now.x!=old.x||now.y!=old.y||now.z!=old.z){std::fprintf(stderr,"FAIL: unrelated vertex changed\n");return 1;}
  }
  std::printf("aspect %.5f marker vertices %u baseline moved %u max_dx %.6f: fixed native coordinates, other vertices unchanged\n",aspect,count,moved,displacement);
 }
}
