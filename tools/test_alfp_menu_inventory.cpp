// Compares host menu readers with the original ALFP Thumb routines on copied
// GBAS memory. This is an interpreter/reference check, not device/visual QA.
/* g++ -std=c++20 -O2 -Isrc -Igbarecomp/src/armv4t tools/test_alfp_menu_inventory.cpp
   build-europe/gbarecomp_build/libgbarecomp_armv4t.a -o /tmp/test_alfp_menu_inventory */
#include "legacy2_menu_items.h"
#include "legacy2_menu_inventory.h"
#include "legacy2_menu_journal.h"
#include "thumb_decode.h"
#include "arm_decode.h"
#include "interpreter.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
using Bytes = std::vector<std::uint8_t>;
using namespace legacy2::menu;
static Bytes load(const char* p) {
    std::ifstream f(p, std::ios::binary); assert(f.good());
    return {std::istreambuf_iterator<char>(f), {}};
}
struct ReferenceBus : armv4t::Bus {
    Bytes rom, iwram, ewram;
    std::uint8_t read8(std::uint32_t a) override {
        if (a >= 0x08000000 && a - 0x08000000 < rom.size()) return rom[a - 0x08000000];
        if (a >= 0x03000000 && a - 0x03000000 < iwram.size()) return iwram[a - 0x03000000];
        if (a >= 0x02000000 && a - 0x02000000 < ewram.size()) return ewram[a - 0x02000000];
        std::cerr << "unsupported read " << std::hex << a << '\n';
        throw std::runtime_error("reference left ROM/RAM");
    }
    std::uint16_t read16(std::uint32_t a) override { return read8(a) | read8(a+1)<<8; }
    std::uint32_t read32(std::uint32_t a) override { return read16(a) | std::uint32_t(read16(a+2))<<16; }
    void write8(std::uint32_t a, std::uint8_t v) override {
        if (a >= 0x03000000 && a-0x03000000 < iwram.size()) iwram[a-0x03000000]=v;
        else if (a >= 0x02000000 && a-0x02000000 < ewram.size()) ewram[a-0x02000000]=v;
        else throw std::runtime_error("reference write outside RAM");
    }
    void write16(std::uint32_t a,std::uint16_t v) override { write8(a,v); write8(a+1,v>>8); }
    void write32(std::uint32_t a,std::uint32_t v) override { write16(a,v); write16(a+2,v>>16); }
    std::uint32_t call(std::uint32_t pc,std::uint32_t a=0,std::uint32_t b=0,std::uint32_t c=0) {
        armv4t::CPUState cpu{};
        cpu.thumb=cpu.cpsr.t=true; cpu.cpsr.mode=0x1f;
        cpu.R[0]=a;cpu.R[1]=b;cpu.R[2]=c;
        cpu.R[13]=0x03007c00;cpu.R[14]=0x0f000001;cpu.R[15]=pc;
        for(unsigned steps=0;steps<200000;++steps) {
            if(cpu.R[15]==0x0f000000) return cpu.R[0];
            pc=cpu.R[15];
            auto ins=cpu.thumb ? armv4t::ThumbDecoder::decode(read16(pc),pc)
                              : armv4t::ArmDecoder::decode(read32(pc),pc);
            auto result=armv4t::Interpreter::step(cpu,*this,ins);
            if(result!=armv4t::Interpreter::Result::Normal && result!=armv4t::Interpreter::Result::Branched) {
                std::cerr<<"reference failure PC="<<std::hex<<pc<<" result="<<int(result)<<'\n';
                throw std::runtime_error("reference instruction failure");
            }
        }
        throw std::runtime_error("reference instruction limit");
    }
};
static void put(Bytes& v,unsigned p,std::uint32_t n) { for(unsigned i=0;i<4;++i)v.at(p+i)=n>>(i*8); }
static unsigned compare_inventory(ReferenceBus& bus) {
    const auto iwram_before=bus.iwram,ewram_before=bus.ewram,rom_before=bus.rom;
    auto counts=decode_item_counts(bus.iwram.data(),bus.iwram.size(),bus.ewram.data(),bus.ewram.size(),bus.rom.data(),bus.rom.size());
    assert(bus.iwram==iwram_before && bus.ewram==ewram_before && bus.rom==rom_before);
    for(unsigned id=0;id<46;++id) {
        bus.write32(0x03007b00,1);bus.write32(0x03007b04,id);
        bus.call(0x0800648e,0x03007b00);
        const auto actual=bus.read32(0x03007b04);
        if(counts[id]!=int(actual)) {std::cerr<<"inventory mismatch id="<<id<<" host="<<counts[id]<<" native="<<actual<<'\n';assert(false);}
    }
    return 46;
}
static unsigned compare_journal(ReferenceBus& bus) {
    auto counts=decode_item_counts(bus.iwram.data(),bus.iwram.size(),bus.ewram.data(),bus.ewram.size(),bus.rom.data(),bus.rom.size());
    const auto iwram_before=bus.iwram,ewram_before=bus.ewram,rom_before=bus.rom;
    auto journal=decode_journal(bus.iwram.data(),bus.iwram.size(),bus.rom.data(),bus.rom.size(),&counts);
    assert(bus.iwram==iwram_before && bus.ewram==ewram_before && bus.rom==rom_before);
    for(unsigned id=0;id<43;++id) {
        const auto record=kJournalTable+id*32;
        auto condition=[&](unsigned offset) {
            bus.write32(0x03007b00,0);
            bus.call(0x08008308,0x03007b00,read32(bus.rom.data()+record+offset),0);
            return bus.read32(0x03007b04)!=0;
        };
        int status=condition(4)?2:condition(8)?1:0;
        if(journal[id].status!=status) {std::cerr<<"journal mismatch id="<<id<<" host="<<int(journal[id].status)<<" native="<<status<<'\n';assert(false);}
        assert(!journal[id].text.empty());assert(journal[id].category<=2);
    }
    return 43;
}
int main(int argc,char** argv) {
    assert(argc==3); ReferenceBus bus;bus.rom=load(argv[1]);
    assert(bus.rom.size()==0x800000 && std::string(bus.rom.begin()+0xac,bus.rom.begin()+0xb0)=="ALFP");
    auto state=load(argv[2]);assert(state.size()>=52 && std::string(state.begin(),state.begin()+4)=="GBAS");
    assert(std::string(state.begin()+8,state.begin()+48)=="db36ff52fcd63f753f9d66439aa3d2216701c326");
    std::size_t p=52;Bytes memory;
    for(unsigned i=0;i<read32(state.data()+48);++i) {
        assert(p+8<=state.size());auto n=read32(state.data()+p+4);assert(p+8+n<=state.size());
        if(std::string(state.begin()+p,state.begin()+p+4)=="BUS0")memory.assign(state.begin()+p+8,state.begin()+p+8+n);
        p+=8+n;
    }
    assert(memory.size()>=0x48000);bus.ewram.assign(memory.begin(),memory.begin()+0x40000);bus.iwram.assign(memory.begin()+0x40000,memory.begin()+0x48000);
    const Bytes rom_before=bus.rom,iwram_before=bus.iwram,ewram_before=bus.ewram;
    const auto catalog=decode_item_catalog(bus.rom.data(),bus.rom.size());
    for(unsigned id=0;id<46;++id){
        assert(catalog[id].valid);
        // Cross-check the exact original menu's lookup routine 08026908.
        const auto definition=kItemDefinitionTable+id*kItemDefinitionStride;
        const auto name_id=read16(bus.rom.data()+definition+4),description_id=read16(bus.rom.data()+definition+6);
        const auto n=bus.call(0x08026908,name_id,4),d=bus.call(0x08026908,description_id,4);
        assert(catalog[id].name==read_item_text(bus.rom.data(),bus.rom.size(),n));
        assert(catalog[id].description==read_item_text(bus.rom.data(),bus.rom.size(),d));
    }
    assert(catalog[0].name==u"Alubia Senzu" && catalog[2].name==u"Cáp. FRZ +1");
    assert(catalog[37].name==u"Libro de Matemáticas" && catalog[45].name==u"Radar Dragón");
    unsigned inventory_cases=compare_inventory(bus),journal_cases=compare_journal(bus);
    // Readers above are checked for exact nonmutation before invoking the
    // original routine. The reference VM itself writes its current-context
    // pointer at 03001054 and our isolated scratch/stack above 03007B00.
    // All of those writes affect this process's memory copy only.
    for(unsigned i=0;i<0x7b00;++i)
        if(i<0x1054 || i>=0x1058)assert(bus.iwram[i]==iwram_before[i]);
    assert(bus.ewram==ewram_before && bus.rom==rom_before);
    // Exercise both inline collections with actual pickup vtables and IDs.
    bus.iwram=iwram_before;bus.ewram=ewram_before;
    std::fill(bus.iwram.begin()+0x13b0,bus.iwram.begin()+0x13b0+0x810,0);
    for(unsigned list=0;list<2;++list){unsigned count=0x13b0+(list?0x40c:4);put(bus.iwram,count,2);
        for(unsigned i=0;i<2;++i){unsigned a=0x30000+list*0x100+i*0x50;put(bus.iwram,count+4+i*4,0x02000000+a);put(bus.ewram,a,0x0802513c);put(bus.ewram,a+0x40,22+i);}}
    const auto both_lists=decode_item_counts(bus.iwram.data(),bus.iwram.size(),bus.ewram.data(),bus.ewram.size(),bus.rom.data(),bus.rom.size());
    assert(both_lists[22]==int(bus.iwram[0x11d7+22])+2);
    assert(both_lists[23]==int(bus.iwram[0x11d7+23])+2);
    inventory_cases+=compare_inventory(bus);journal_cases+=compare_journal(bus);
    // Each authored flag is independently toggled, plus mixed inventory counts,
    // so AND/OR, extended integer flag 298, and item-dependent missions run.
    for(unsigned trial=0;trial<320;++trial){
        bus.iwram=iwram_before;bus.ewram=ewram_before;
        std::fill(bus.iwram.begin()+0x1160,bus.iwram.begin()+0x11d7,0);
        if(trial<300)bus.iwram[0x1160+trial/8]=1u<<(trial%8);
        else {for(unsigned i=0;i<119;++i)bus.iwram[0x1160+i]=(trial*37u+i*73u)^((trial&1)?255:0);}
        for(unsigned id=0;id<46;++id)bus.iwram[0x11d7+id]=(trial+id)%4==0?0:((trial+id)%4==1?1:255);
        inventory_cases+=compare_inventory(bus);journal_cases+=compare_journal(bus);
    }
    // Unknown snapshots must not fabricate an empty inventory or completed missions.
    auto absent=decode_item_counts(nullptr,0,nullptr,0,bus.rom.data(),bus.rom.size());for(int n:absent)assert(n==-1);
    auto unknown=decode_journal(nullptr,0,bus.rom.data(),bus.rom.size());for(auto& j:unknown)assert(j.status==-1);
    std::cout<<"PASS: 46 catalog names/descriptions against original text lookup; "<<inventory_cases<<" item quantities and "<<journal_cases<<" mission statuses against original ALFP Thumb routines (real copied snapshot + fixtures). No ROM/save files modified.\n";
}
