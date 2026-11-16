#include "/home/plex-rober/legacy-goku2-thor/src/legacy2_extended_view_europe.cpp"
#include "snapshot.h"
#include <fstream>
#include <iostream>
#include <vector>
gba::GbaBus* test_bus;
namespace gbarecomp {gba::GbaBus* active_bus(){return test_bus;}}
extern "C" {unsigned g_ws_active=1,g_ws_extra_left=22,g_ws_extra_right=22;RuntimeBusReadOverride g_runtime_bus_read_override=nullptr;unsigned long long g_runtime_cycles=0;std::uint32_t runtime_current_pc(){return 0;}void legacy2_menu_publish_language(int){}void legacy2_menu_publish_probe(const std::uint8_t*){}}
std::vector<unsigned char> load(const char*p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){auto rom=load(argv[1]),state=load(argv[2]);gba::GbaBus bus;test_bus=&bus;bus.set_rom(rom.data(),rom.size());auto u=[](const unsigned char*p){return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);};for(unsigned o=52;o+8<state.size();){auto n=u(state.data()+o+4);gbarecomp::debug::SnapshotReader r(state.data()+o+8,n);std::string tag((char*)state.data()+o,4);if(tag=="BUS0")bus.deserialize(r);if(tag=="IO_0")bus.io().deserialize(r);o+=8+n;}
 using namespace legacy2;resolve_field_objects(&bus);auto level=mem32(&bus,0x03000f80);std::cout<<std::hex<<"DISPCNT "<<bus.io().read16(0)<<" level "<<level<<" world "<<is_overworld(&bus)<<"\n";
 for(int b=0;b<4;b++){auto resource=mem32(&bus,level+0x14+b*4);std::cout<<"BG "<<b<<" cnt "<<bus.io().read16(8+b*2)<<" scroll "<<bus.io().read16(16+b*4)<<","<<bus.io().read16(18+b*4)<<" desc "<<resource<<" ctor "<<mem32(&bus,resource)<<" object "<<g_field_objects[b]<<"\n";}
 for(unsigned off=0;off+0x2030<=0x40000;off+=4){unsigned ob=0x02000000+off;if(field_object_valid(&bus,ob,0)&&ewram32(&bus,ob+0x10)==mem32(&bus,level+0x14)){std::cout<<"BG0 CANDIDATE "<<ob<<" slots "<<unsigned(mem8(&bus,ob+0x24))<<" "<<unsigned(mem8(&bus,ob+0x25))<<"\n";for(int yy=20;yy<90;yy+=8){for(int xx=-22;xx<24;xx+=8){std::uint16_t v=0;field_entry(&bus,ob,0x1b8+xx,0xb7+yy,&v);std::cout<<v<<" ";}std::cout<<"\n";}}} auto*oa=bus.oam_ptr();for(int i=0;i<128;i++){unsigned a=oa[i*8]|oa[i*8+1]<<8,b=oa[i*8+2]|oa[i*8+3]<<8,c=oa[i*8+4]|oa[i*8+5]<<8;if((a&0x300)!=0x200)std::cout<<"OAM "<<i<<" "<<a<<" "<<b<<" "<<c<<"\n";}
}
