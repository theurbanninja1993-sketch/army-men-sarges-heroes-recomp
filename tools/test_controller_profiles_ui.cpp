#include "menu_ui.h"
#include "frontend_common.h"
#include "soft_render.h"
#include "dreamcomp/port.h"
#include "dreamcomp/settings.h"
#include "dreamcomp/controller_mode.h"
#include "dream/render/png.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <cstdio>
#include <cstdlib>
using namespace dreamcomp::frontend;
static void check(bool yes,const char* message) {
    if (!yes) { std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1); }
}
struct TestSystem final : Rml::SystemInterface {
    double GetElapsedTime() override { return double(SDL_GetTicksNS())*1e-9; }
};
int main(int argc,char** argv) {
    check(argc==4,"arguments: frontend directory, scratch settings, screenshot");
    check(SDL_Init(SDL_INIT_GAMEPAD),SDL_GetError());
    Utf8FileInterface files;TestSystem system;SoftRenderer renderer;
    Rml::SetFileInterface(&files);Rml::SetSystemInterface(&system);Rml::SetRenderInterface(&renderer);
    check(Rml::Initialise(),"initialize RmlUi");
    std::string error;
    check(load_fonts(argv[1],&error),error.c_str());
    auto* context=Rml::CreateContext("profile-test",{1280,720});
    dreamcomp::Settings settings;settings.load(argv[2]);
    dreamcomp::PortInfo port;port.id="test";port.title="Army Men controls";port.controller_camera=true;
    UiConfig cfg{&settings,&port,{},argv[1],"Profile verification"};
    unsigned applied=0;
    UiHost host;host.applied=[&]{++applied;};
    host.bindings_path=[&]{return std::string(argv[2])+".bindings.ini";};
    {
        MenuUi ui(cfg,Mode::InGame,host);check(ui.load(context,&error),error.c_str());
        ui.set_pads({});ui.show("settings/controls");ui.tick();
        auto click=[&](const std::string& id) {
            auto* el=ui.element(id);check(el!=nullptr, ("missing element "+id).c_str());
            el->DispatchEvent(Rml::EventId::Click,{});ui.tick();context->Update();
        };
        click("o-controller_mode-modern");
        click("c-edit-1");
        check(ui.element("o-controller_mode_p2-inherit")->IsClassSet("sel"),"disconnected player2 defaults to inheritance");
        check(ui.element("btn-apply")!=nullptr,"profile exposes Apply");
        click("o-controller_mode_p2-classic");click("btn-apply");
        check(settings.get("controller_mode")=="modern" && settings.get("controller_mode_p2")=="classic","Apply saves only player2 override and main choice");
        renderer.begin(1280,720);context->Render();
        const auto png=dream::render::png::encode(renderer.pixels().data(),1280,720);
        std::ofstream(argv[3],std::ios::binary).write(reinterpret_cast<const char*>(png.data()),static_cast<std::streamsize>(png.size()));
        click("c-back");click("c-edit-2");
        check(ui.element("o-controller_mode_p3-inherit")->IsClassSet("sel"),"player3 does not inherit player2 override");
        click("o-controller_mode_p3-modern");click("btn-apply");
        click("c-back");click("c-edit-3");
        check(ui.element("o-controller_mode_p4-inherit")->IsClassSet("sel"),"player4 mode is independent");
        click("c-back");click("o-controller_mode-classic");click("btn-apply");
        check(!dreamcomp::controller_is_modern(settings,3) && dreamcomp::controller_is_modern(settings,2),"main change affects inheritance while explicit Modern persists");
        click("c-edit-1");click("o-controller_mode_p2-inherit");click("btn-apply");
        check(settings.get("controller_mode_p2")=="inherit","Use main clears explicit override");
        check(applied==4,"each Apply notifies live game settings callback");
    }
    Rml::RemoveContext("profile-test");Rml::Shutdown();SDL_Quit();
    dreamcomp::Settings loaded;loaded.load(argv[2]);
    check(loaded.get("controller_mode_p3")=="modern" && loaded.get("controller_mode_p2")=="inherit","UI changes persist on disk");
    std::puts("PASS: actual profile UI actions, four independent modes, Apply callback, inheritance and persistence");
}
