// Route verified PAL discs to their separately translated executable. Never accept
// a PAL boot image in the NTSC runtime: the executable layouts are different.
#include "dreamcomp/settings.h"
#include "dreamcomp/port.h"
#include "dream/runtime/gdrom/disc.h"
#include "dream/runtime/sha1.h"
#include <SDL3/SDL.h>
#include <filesystem>
#include <cstdio>
#include <cstdlib>

namespace {
std::string value(const std::vector<std::string>& args, const char* key) {
    for (std::size_t i=1;i+1<args.size();++i) if(args[i]==key) return args[i+1];
    return {};
}
void set(std::vector<std::string>& args,const char* key,const std::string& v) {
    for(std::size_t i=1;i+1<args.size();++i) if(args[i]==key){args[i+1]=v;return;}
    args.push_back(key);args.push_back(v);
}
}
void army_prepare_variant(std::vector<std::string>& args) {
    const auto image=value(args,"--disc");
    if(image.empty()) return;
    std::string error;
    auto disc=dream::gdrom::open_disc(std::filesystem::u8path(image),error);
    std::vector<std::uint8_t> bytes;
    if(!disc || !dream::gdrom::read_root_file(*disc,"",bytes)) return;
    if(dream::sha1_hex(bytes.data(),bytes.size())!="7e1288a58f26eedf76cc7bdbaeb53a72a9332fbc") return;
    // The secondary game/transfer payloads matter too; a matching loader alone
    // does not establish support for a modified PAL disc.
    const std::pair<const char*,const char*> files[]={
        {"KLINK/KLINKENGLISH60HZ.BIN","7e81e3336ab4d5dd6c7094f00e388ef4d9d12192"},
        {"GINXFER.BIN","08e56771a4b0ff8dd6cf2896902e94fed7402fd0"}};
    for(const auto& [file,sha]:files) {
        if(!dream::gdrom::read_root_file(*disc,file,bytes) || dream::sha1_hex(bytes.data(),bytes.size())!=sha) {
            std::fprintf(stderr,"PAL preview: unsupported or missing %s; use the verified PAL revision.\n",file);
            std::exit(3);
        }
    }
    auto dir=std::filesystem::absolute(std::filesystem::u8path(args[0])).parent_path();
    if(!std::filesystem::exists(std::filesystem::u8path(args[0])))
        if(const char* base=SDL_GetBasePath()) dir=std::filesystem::u8path(base);
#ifdef _WIN32
    const auto program=dir/"armymensh_pal_controls.exe";
#else
    const auto program=dir/"armymensh_pal_controls";
#endif
    auto config=dir/"armymensh_pal_probe.toml";
    if(!std::filesystem::exists(config)) config=dir.parent_path().parent_path()/"game/armymensh_pal_probe.toml";
    if(!std::filesystem::exists(program) || !std::filesystem::exists(config)) {
        std::fprintf(stderr,"PAL preview: this package has no PAL runtime. Install the Linux PAL preview package.\nExpected runtime: %s\nConfig: %s\n",program.string().c_str(),config.string().c_str());
        std::exit(3);
    }
    auto child=args;
    child[0]=program.string();
    set(child,"--config",config.string());
    // Preserve the launcher's settings and memory-card selection across regions.
    set(child,"--settings",dreamcomp::settings().file().string());
    const auto usa_vmu = dreamcomp::default_config_dir("armymensh")/"vmu_a1.bin";
    if(value(child,"--vmu")==usa_vmu.string()) {
        const auto pal_vmu=dreamcomp::default_config_dir("armymensh_pal_en60")/"vmu_a1.bin";
        std::filesystem::create_directories(pal_vmu.parent_path());
        set(child,"--vmu",pal_vmu.string());
    }
    child.push_back("--no-launcher");
    std::vector<const char*> argv;
    for(const auto& arg:child) argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    std::fprintf(stderr,"PAL preview: loading English / 60 Hz. Other languages and 50 Hz are unsupported.\n");
    std::fflush(nullptr);
    SDL_Process* process=SDL_CreateProcess(argv.data(),false);
    if(!process){std::fprintf(stderr,"PAL runtime: %s\n",SDL_GetError());std::exit(1);}
    int status=1;
    if(!SDL_WaitProcess(process,true,&status))std::fprintf(stderr,"PAL runtime: %s\n",SDL_GetError());
    SDL_DestroyProcess(process);
    std::exit(status);
}
