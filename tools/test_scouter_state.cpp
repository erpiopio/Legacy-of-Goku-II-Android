#include "legacy2_scouter_state.h"
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include <iostream>
int main(int argc,char**argv){
    if(argc!=2)return 2;
    std::ifstream in(argv[1],std::ios::binary);
    std::vector<std::uint8_t> rom((std::istreambuf_iterator<char>(in)),{});
    std::vector<std::uint8_t> iw(0x8000), ew(0x40000);
    auto set=[](std::vector<std::uint8_t>& b,unsigned o,unsigned v){for(unsigned i=0;i<4;++i)b[o+i]=v>>(i*8);};
    set(iw,0xF0C,258);set(iw,0xF84,1000);set(iw,0xF88,1000);
    set(iw,0x1FA8,0x02000000);set(ew,4,1);set(ew,8,0x02001000);
    // A synthetic actor/query fixture; the regional table remains real ALFP.
    set(ew,0x1000,0x08000100);set(rom,0x124,0x08015A31-0x08000100);
    for(unsigned p:{0u,500u,1000u}){
        set(ew,0x1158,p*256);set(ew,0x115C,p*256);
        auto s=legacy2::scouter::decode(iw.data(),iw.size(),ew.data(),ew.size(),rom.data(),rom.size(),9);
        assert(s[0]==1&&s[1]==0&&s[2]==258&&s[7]==144+int(p*80/1000)&&s[8]==96+int(p*80/1000)&&s[10]==9);
    }
    set(iw,0xF84,0);
    auto s=legacy2::scouter::decode(iw.data(),iw.size(),ew.data(),ew.size(),rom.data(),rom.size(),10);
    assert(!s[0]&&s[1]==-1);
    std::cout<<"ALFP real region + synthetic actor: endpoints, midpoint, generation, invalid dimensions passed\n";
}
