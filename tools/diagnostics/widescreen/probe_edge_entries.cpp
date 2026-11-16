#define main production_hook_fixture_main
#include "/home/plex-rober/legacy-goku2-thor/tools/test_alfp_view_hooks.cpp"
#undef main
#include "snapshot.h"
#include <fstream>
#include <iterator>
#include <string>
std::vector<unsigned char> read_file(const char* p) {
    std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};
}
int main(int argc,char**argv) {
    if(argc!=3)return 2;
    auto rom=read_file(argv[1]),state=read_file(argv[2]);
    gba::GbaBus bus;test_bus=&bus;bus.set_rom(rom.data(),rom.size());
    auto u=[](const unsigned char*p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);};
    for(unsigned o=52;o+8<state.size();) {
        auto n=u(state.data()+o+4);gbarecomp::debug::SnapshotReader r(state.data()+o+8,n);
        std::string tag((char*)state.data()+o,4);
        if(tag=="BUS0")bus.deserialize(r);if(tag=="IO_0")bus.io().deserialize(r);o+=8+n;
    }
    g_ws_active=1;legacy2::install_extended_view(0,0);legacy2::resolve_field_objects(&bus);
    for(int bg=0;bg<4;++bg) {
        unsigned differences=0,compared=0,printed=0;
        for(int y=0;y<160;++y)for(int x=0;x<284;++x) {
            if(x>=22&&x<262)continue;
            int hx=x-22;legacy2::legacy2_bg_x_provider(bg,x,y,&hx);
            std::uint16_t field=0;
            if(!legacy2::field_screen_entry(&bus,bg,hx,y,&field))continue;
            int sx=(bus.io().read16(16+bg*4)+hx)&255;
            int sy=(bus.io().read16(18+bg*4)+y)&255;
            unsigned base=((bus.io().read16(8+bg*2)>>8)&31)*0x800;
            auto* v=bus.vram_ptr()+base+((sy>>3)*32+(sx>>3))*2;
            unsigned native=v[0]|v[1]<<8;++compared;
            if(native!=field) {
                ++differences;
                if(printed<8&&y%8==0) {
                    std::cout<<"DIFF BG="<<bg<<" output="<<x<<","<<y<<" sample="<<hx<<" VRAM="<<std::hex<<native<<" FIELD="<<field<<std::dec<<"\n";++printed;
                }
            }
        }
        std::cout<<"BG "<<bg<<" differences="<<differences<<" / "<<compared<<"\n";
    }
}
