// Production-hook integration test with real GbaBus/GbaIo/GbaPpu globals.
// The small ROM below is an explicitly synthetic descriptor fixture. This
// checks hook routing/nonmutation, not ROM geometry or a live flight scene.
/* Build from the project root:
   g++ -std=c++20 -O1 -Isrc -Igbarecomp/src/gba -Igbarecomp/src/runtime
     -Igbarecomp/src/armv4t -Igbarecomp/src/debug tools/test_alfp_view_hooks.cpp
     -Wl,--start-group build-europe/gbarecomp_build/libgbarecomp_gba.a
     build-europe/gbarecomp_build/libgbarecomp_armv4t.a
     build-europe/gbarecomp_build/libgbarecomp_debug.a -Wl,--end-group
     -pthread -o /tmp/test_alfp_view_hooks
*/
#include "../src/legacy2_extended_view_europe.cpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
gba::GbaBus* test_bus=nullptr;
unsigned published=0;
void require(bool value,const char* message) {
    if(!value)throw std::runtime_error(message);
}
void put32(std::uint8_t* p,std::uint32_t value) {
    for(unsigned i=0;i<4;++i)p[i]=value>>(i*8);
}
}
namespace gbarecomp {gba::GbaBus* active_bus(){return test_bus;}}
extern "C" {
ArmCpuState g_cpu = {};
unsigned g_ws_active=0,g_ws_extra_left=22,g_ws_extra_right=22;
RuntimeBusReadOverride g_runtime_bus_read_override=nullptr;
unsigned long long g_runtime_cycles=0;
std::uint32_t runtime_current_pc(){return 0;}
void legacy2_menu_publish_language(int){}
void legacy2_menu_publish_probe(const std::uint8_t* p){
    require(test_bus && p==test_bus->iwram_ptr(),"publication did not use active bus");++published;
}
}
int main() {
 try {
    gba::GbaBus bus;test_bus=&bus;
    std::vector<std::uint8_t> rom(0x1000,0);
    put32(rom.data()+0x23c,1); // Authored descriptor fixture with projected map.
    bus.set_rom(rom.data(),rom.size());
    auto* iw=bus.iwram_ptr();
    put32(iw+0xf80,0x08000200);
    put32(iw+0xf68,1024);put32(iw+0xf6c,0);
    put32(iw+0xf70,0);put32(iw+0xf74,1024);
    bus.io().write16(0,0x1741); // Native ALFP projected video mode.
    const std::vector<std::uint8_t> iw_before(iw,iw+0x8000);
    const std::vector<std::uint8_t> ew_before(bus.ewram_ptr(),bus.ewram_ptr()+0x40000);
    g_ws_active=1;
    legacy2::install_extended_view(0,0);
    require(gba::g_ws_obj_attr_x_provider==legacy2::legacy2_obj_x_provider,
            "production OAM provider was not installed");
    require(gba::g_ws_tilemap_provider==legacy2::legacy2_tilemap_provider,
            "production tilemap provider was not installed");
    require(gba::g_ws_bg_x_provider==legacy2::legacy2_bg_x_provider,
            "production BG provider was not installed");
    require(g_runtime_bus_read_override==legacy2::legacy2_bus_read_override,
            "production read hook was not installed");
    struct Read {std::uint32_t pc,address;int expected;};
    const Read reads[]={
        {0x0801c83a,0x03000f3c,77},{0x0801c840,0x03000f44,123},
        {0x0801c848,0x03000f40,99},{0x0801c84e,0x03000f48,101}};
    unsigned cases=0;
    for(const auto& read:reads){
        std::uint32_t out=0xdeadbeef;
        require(g_runtime_bus_read_override(read.pc,read.address,4,100,&out)==1 &&
                out==std::uint32_t(read.expected),"exact projected read routing");++cases;
        for(unsigned width:{1u,2u,8u}){
            out=0xdeadbeef;require(!g_runtime_bus_read_override(read.pc,read.address,width,100,&out)&&
                out==0xdeadbeef,"nonword read changed");++cases;
        }
        out=0xdeadbeef;
        require(!g_runtime_bus_read_override(read.pc+2,read.address,4,100,&out)&&out==0xdeadbeef,
                "unrelated PC read changed");++cases;
        require(!g_runtime_bus_read_override(read.pc,read.address+4,4,100,&out)&&out==0xdeadbeef,
                "unrelated address read changed");++cases;
        require(!g_runtime_bus_read_override(read.pc,read.address,4,100,nullptr),"null read result accepted");++cases;
    }
    put32(iw+0xf68,0);put32(iw+0xf74,0);
    for(const auto& read:reads){std::uint32_t unchanged=0xdeadbeef;
        require(!g_runtime_bus_read_override(read.pc,read.address,4,100,&unchanged)&&unchanged==0xdeadbeef,
                "uninitialized camera changed visibility");++cases;}
    // Rotated camera extends the other world axis; these exercise the actual
    // production reads rather than a duplicated margin formula.
    put32(iw+0xf68,0);put32(iw+0xf6c,1024);
    put32(iw+0xf70,std::uint32_t(-1024));put32(iw+0xf74,0);
    std::uint32_t out=0;
    require(g_runtime_bus_read_override(0x0801c83a,0x03000f3c,4,100,&out)&&out==99,
            "rotated X inverse margin");
    require(g_runtime_bus_read_override(0x0801c848,0x03000f40,4,100,&out)&&out==77,
            "rotated Y inverse margin");cases+=2;
    for(int x=256;x<=262;++x){
        int mapped=999;
        require(gba::g_ws_obj_attr_x_provider(0,0x0300|120,std::uint16_t(x),0x308,&mapped)&&mapped==x,
                "positive flight OAM right margin wrapped");++cases;
    }
    for(auto descriptor:{std::pair<std::uint16_t,std::uint16_t>{std::uint16_t(0x0300|120),0x308},
                        std::pair<std::uint16_t,std::uint16_t>{std::uint16_t(0x0300|16),0x358}}){
        int mapped=999;
        require(gba::g_ws_obj_attr_x_provider(0,descriptor.first,506,descriptor.second,&mapped)&&mapped==-6,
                "affine flight actor was mistaken for HUD");++cases;
    }
    require(std::equal(ew_before.begin(),ew_before.end(),bus.ewram_ptr()),"hook changed EWRAM");
    // Restore our matrix fixture before checking read-only IWRAM.
    put32(iw+0xf68,1024);put32(iw+0xf6c,0);
    put32(iw+0xf70,0);put32(iw+0xf74,1024);
    require(std::equal(iw_before.begin(),iw_before.end(),iw),"hook changed IWRAM");
    require(rom[0x23c]==1 && std::count(rom.begin(),rom.end(),std::uint8_t(0))==4095,
            "hook changed synthetic ROM");
    // Verified non-affine field HUD semantics remain unchanged.
    bus.io().write16(0,0x1f40);
    for(unsigned bg=0;bg<4;++bg)bus.io().write16(8+bg*2,(28+bg)<<8);
    int mapped=999;
    require(gba::g_ws_obj_attr_x_provider(0,16,16,0x358,&mapped)&&mapped==16,
            "field health HUD moved outside native viewport");
    require(gba::g_ws_obj_attr_x_provider(0,120,120,0x308,&mapped)&&mapped==120,
            "field area-name HUD mapping changed");
    require(gba::g_ws_obj_attr_x_provider(0,0x0300|120,506,0x308,&mapped)&&mapped==-6,
            "affine ground actor was mistaken for HUD");cases+=3;
    // Cover the draw-only common virtual context and reject other callers.
    g_cpu.R[14]=0x0801E313;
    require(g_runtime_bus_read_override(0x08001234,0x03000f3c,4,100,&out)&&out==78,
            "common draw visibility left bound");
    require(g_runtime_bus_read_override(0x08001234,0x03000f44,4,340,&out)&&out==362,
            "common draw visibility right bound");
    g_cpu.R[14]=0x0801E415;
    out=0xdeadbeef;
    require(!g_runtime_bus_read_override(0x08001234,0x03000f3c,4,100,&out)&&out==0xdeadbeef,
            "nonvisibility caller changed");
    for(auto pc:{0x0801534Eu,0x0801615Cu,0x0801D21Eu})
        require(g_runtime_bus_read_override(pc,0x03000f3c,4,100,&out)&&out==78,
                "known draw visibility left bound");
    for(auto pc:{0x08015348u,0x08016156u,0x0801D218u})
        require(g_runtime_bus_read_override(pc,0x03000f44,4,340,&out)&&out==362,
                "known draw visibility right bound");
    cases+=9;
    // Keep production's conservative exterior policy active in the fixture.
    constexpr unsigned fixture_level=0x002A9C3C;
    constexpr unsigned indoor_level=0x002D2B44;
    rom.resize(indoor_level+0x60,0);bus.set_rom(rom.data(),rom.size());
    put32(iw+0xf80,0x08000000+fixture_level);
    put32(rom.data()+fixture_level+0x3c,1);
    // Finite-room extension remaps only BG pixels outside native 240.
    put32(iw+0xf84,256);put32(iw+0xf3c,16);
    put32(rom.data()+fixture_level+0x18,0x08000400);
    put32(rom.data()+0x400,0x080056F9);
    put32(rom.data()+0x404,1024);
    rom[0x414]=1;rom[0x415]=1;
    bus.io().write16(0x14,16);
    mapped=999;
    require(gba::g_ws_bg_x_provider(1,0,80,&mapped)&&mapped==-11,
            "left finite-room visual extension");
    require(gba::g_ws_bg_x_provider(1,283,80,&mapped)&&mapped==218,
            "right finite-room visual extension");
    mapped=999;
    require(!gba::g_ws_bg_x_provider(1,22,80,&mapped)&&mapped==999,
            "finite-room extension changed native pixel");
    // Match the real slow-parallax origin while terrain camera remains 16.
    for(unsigned origin=0;origin<=16;++origin) {
        bus.io().write16(0x14,origin);
        for(int output=0;output<22;++output) {
            int source=output-22;
            gba::g_ws_bg_x_provider(1,output,80,&source);
            require(source+int(origin)>=0&&source+int(origin)<256,
                    "parallax extension sampled outside its resource");
            ++cases;
        }
    }
    bus.io().write16(0x14,16);
    put32(iw+0xf84,512);
    require(!gba::g_ws_bg_x_provider(1,283,80,&mapped)&&mapped==999,
            "short foreground invented terrain inside the real room");
    cases+=4;
    require(gba::g_ws_bg_x_provider(1,0,80,&mapped)&&mapped==-11,
            "large map left edge was not visually extended");
    put32(iw+0xf3c,64);
    bus.io().write16(0x14,64);
    mapped=999;
    require(!gba::g_ws_bg_x_provider(1,0,80,&mapped)&&mapped==999,
            "valid left terrain was reflected");
    require(!gba::g_ws_bg_x_provider(1,22,80,&mapped)&&mapped==999,
            "left extension changed the native viewport");
    bus.io().write16(0x14,16);put32(iw+0xf3c,16);
    cases+=3;
    rom[0x414]=2; // Two real chunks, matching the wider room fixture.
    bus.io().write16(0x14,64);
    mapped=999;
    require(!gba::g_ws_bg_x_provider(1,283,80,&mapped)&&mapped==999,
            "valid right terrain was reflected");
    bus.io().write16(0x14,272);put32(iw+0xf3c,272); // Native right camera limit.
    require(gba::g_ws_bg_x_provider(1,283,80,&mapped)&&mapped==218,
            "right camera limit continuation failed");
    rom[0x414]=1;bus.io().write16(0x14,16);put32(iw+0xf3c,16);
    cases+=2;
    // Real city geometry: 688px room with per-layer resource padding.
    put32(iw+0xf84,688);put32(iw+0xf3c,0);rom[0x414]=3;
    for(unsigned origin:{64u,128u,192u}) {
        bus.io().write16(0x14,origin);
        put32(rom.data()+0x40c,origin*1024);
        require(gba::g_ws_bg_x_provider(1,0,80,&mapped)&&mapped==21,
                "padded city left edge remained blank");
        ++cases;
    }
    put32(rom.data()+0x40c,64*1024);
    put32(iw+0xf3c,448);bus.io().write16(0x14,512);
    require(gba::g_ws_bg_x_provider(1,283,80,&mapped)&&mapped==218,
            "padded city right edge remained blank");
    put32(iw+0xf3c,5);bus.io().write16(0x14,69);
    require(gba::g_ws_bg_x_provider(1,0,80,&mapped)&&mapped==11,
            "partial city edge was discontinuous");
    put32(iw+0xf3c,32);bus.io().write16(0x14,96);mapped=999;
    require(!gba::g_ws_bg_x_provider(1,0,80,&mapped)&&mapped==999,
            "existing city terrain was reflected");
    rom[0x414]=1;bus.io().write16(0x14,16);put32(iw+0xf3c,16);
    put32(rom.data()+0x40c,0);
    cases+=3;
    // Camera RAM can lead/lag the rendered BG by one frame. Padding must
    // be identical for every live-camera value when BG scroll is unchanged.
    put32(iw+0xf84,584);put32(rom.data()+0x40c,64*1024);
    for(unsigned origin:{75u,78u,397u,408u}) {
        bus.io().write16(0x14,origin);
        for(int output=0;output<284;++output) {
            if(output>=22&&output<262)continue;
            int reference=output-22;
            put32(iw+0xf3c,origin-64);
            gba::g_ws_bg_x_provider(1,output,80,&reference);
            for(int delta:{-1,1}) {
                int actual=output-22;
                put32(iw+0xf3c,origin-64+delta);
                gba::g_ws_bg_x_provider(1,output,80,&actual);
                require(actual==reference,"live camera changed a latched margin sample");
                ++cases;
            }
        }
    }
    put32(rom.data()+0x40c,0);bus.io().write16(0x14,16);
    // A fully valid finite indoor resource must not invent extra rooms.
    put32(rom.data()+indoor_level+0x18,0x08000400);
    put32(iw+0xf80,0x08000000+indoor_level);
    put32(iw+0xf84,240);put32(iw+0xf3c,0);bus.io().write16(0x14,0);
    mapped=999;
    require(!gba::g_ws_bg_x_provider(1,0,80,&mapped)&&mapped==999,
            "indoor left wall/void was extended");
    require(!gba::g_ws_bg_x_provider(1,283,80,&mapped)&&mapped==999,
            "indoor right wall/void was extended");
    require(!legacy2::is_verified_exterior_padding_level(0x08001234),
            "unknown map enabled cosmetic continuation");
    put32(iw+0xf80,0x08000000+fixture_level);
    put32(iw+0xf84,688);put32(iw+0xf3c,16);bus.io().write16(0x14,16);
    cases+=3;
    // No OAM slot, atlas tile or near-corner actor is treated as HUD.
    for(int slot=0;slot<128;++slot) {
        for(auto tile:{0x328u,0x338u,0x340u,0x358u}) {
            mapped=999;
            require(gba::g_ws_obj_attr_x_provider(slot,0xa008,8,tile,&mapped)&&mapped==8,
                    "near-corner actor was displaced as HUD");
            ++cases;
        }
    }
    bus.io().write16(0,0x1741);
    require(gba::g_ws_bg_x_provider(0,0,64,&mapped)&&mapped==0,
            "left projected haze used padding");
    require(gba::g_ws_bg_x_provider(0,283,64,&mapped)&&mapped==239,
            "right projected haze used padding");
    mapped=999;
    require(!gba::g_ws_bg_x_provider(1,0,64,&mapped)&&mapped==999,
            "periodic sky mapping changed");
    require(!gba::g_ws_bg_x_provider(0,22,64,&mapped)&&mapped==999,
            "native haze mapping changed");
    bus.io().write16(0,0x1f40);
    put32(rom.data()+0x400,0x08005F49);
    for(unsigned bg=0;bg<4;++bg)put32(rom.data()+fixture_level+0x14+bg*4,0x08000400);
    require(gba::g_ws_bg_x_provider(1,0,64,&mapped)&&mapped==21,
            "left periodic backdrop continuation");
    require(gba::g_ws_bg_x_provider(2,283,64,&mapped)&&mapped==218,
            "right periodic backdrop continuation");
    cases+=6;
    // Native nebula signature is independent of the stale gameplay level.
    bus.io().write16(0x0e,0x1f83);
    constexpr unsigned nebula[]={0xe0e0e0e0,0xd8cacaca,0xcae0e0e0,0xdfcacaca,
        0xcacae0e0,0xdfdfdfca,0xcacacae0,0xdfdfdfdf};
    for(unsigned i=0;i<8;++i) {
        put32(bus.vram_ptr()+i*4,nebula[i]);
        bus.vram_ptr()[0xf800+i*2]=i;bus.vram_ptr()[0xf801+i*2]=0;
    }
    ++legacy2::g_view_frame;
    require(legacy2::is_native_nebula_menu(&bus),"native nebula not recognized");
    require(!legacy2::is_overworld(&bus),"file selection classified as field");
    require(gba::g_ws_bg_x_provider(3,0,80,&mapped)&&mapped==0,"nebula left scale endpoint");
    require(gba::g_ws_bg_x_provider(3,283,80,&mapped)&&mapped==239,"nebula right scale endpoint");
    require(gba::g_ws_bg_x_provider(2,0,80,&mapped)==-1,"menu ornaments repeated on left");
    require(gba::g_ws_bg_x_provider(1,283,80,&mapped)==-1,"menu frame repeated on right");
    mapped=999;
    require(!gba::g_ws_bg_x_provider(2,22,80,&mapped)&&mapped==999,"native menu layout changed");
    bus.vram_ptr()[5]^=1;++legacy2::g_view_frame;
    require(!legacy2::is_native_nebula_menu(&bus),"unrelated texture identified as nebula");
    std::fill(bus.vram_ptr(),bus.vram_ptr()+32,0);++legacy2::g_view_frame;
    cases+=8;
    // Native off: every enhancement provider declines without changing outputs.
    g_ws_active=0;
    gbarecomp::ExtendedViewFrameInfo info{};
    legacy2::update_extended_view(&info);
    require(published==1,"frame publication did not run");
    require(g_runtime_bus_read_override==nullptr,"native off retained active read hook");
    out=0xdeadbeef;
    require(!legacy2::legacy2_bus_read_override(0x0801c83a,0x03000f3c,4,100,&out)&&out==0xdeadbeef,
            "native off changed read");
    mapped=999;
    require(!gba::g_ws_obj_attr_x_provider(0,0x0300|120,506,0x308,&mapped)&&mapped==999,
            "native off changed OAM");
    require(!gba::g_ws_bg_x_provider(0,0,0,&mapped)&&mapped==999,"native off changed BG coordinate");
    std::uint16_t tile=0xbeef;
    require(gba::g_ws_tilemap_provider(0,0,0,&tile)==gba::kWsTilemapUnavailable&&tile==0xbeef,
            "native off changed tilemap");
    require(!legacy2::legacy2_alfp_cull_left_margin()&&!legacy2::legacy2_alfp_cull_right_margin(),
            "native off changed screen culling");cases+=5;
    test_bus=nullptr;out=0xdeadbeef;
    require(!legacy2::legacy2_bus_read_override(0x0801c83a,0x03000f3c,4,100,&out)&&out==0xdeadbeef,
            "missing bus changed read");
    std::cout<<"PASS: production ALFP view hooks; "<<cases<<" exact-PC/width, inverse rotation, OAM wrap/HUD collision, read-only and native-off cases; real GbaBus/GbaIo/PPU globals, synthetic descriptor.\n";
    return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
