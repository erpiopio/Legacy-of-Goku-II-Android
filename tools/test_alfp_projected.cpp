// Execute original ALFP projected predicate and OAM renderer in an isolated
// ARM7 interpreter. No ROM, generated code, game save, or player state is written.
// This qualifies isolated geometry only; it is not a live flight-scene test.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include "arm_decode.h"
#include "thumb_decode.h"
#include "interpreter.h"
#include "cpu_state.h"
#include "bus.h"
#include "legacy2_projected_view.h"

namespace {
constexpr uint32_t camera=0x03000F30, actor=0x02010000, resource=0x02011000;
constexpr uint32_t context=0x02012000, oam=0x02013000, affine=0x02014000;
constexpr uint32_t sentinel=0x03007FF0;
struct Bus : armv4t::Bus {
  std::vector<uint8_t> rom, ew=std::vector<uint8_t>(0x40000), iw=std::vector<uint8_t>(0x8000);
  uint32_t pc=0; int pad_x=0,pad_y=0; bool wide=false,oob=false;
  uint8_t read8(uint32_t a) override {
    if(a>=0x08000000 && a-0x08000000<rom.size()) return rom[a-0x08000000];
    if(a>=0x02000000 && a<0x02040000) return ew[a-0x02000000];
    if(a>=0x03000000 && a<0x03008000) return iw[a-0x03000000];
    oob=true;return 0;
  }
  uint16_t read16(uint32_t a) override { return uint16_t(read8(a))|uint16_t(read8(a+1))<<8; }
  uint32_t raw32(uint32_t a) { return uint32_t(read16(a))|uint32_t(read16(a+2))<<16; }
  uint32_t read32(uint32_t a) override {
    uint32_t v=raw32(a);
    if(wide) {
      if(pc==0x0801C83A && a==camera+0xC) v-=pad_x;
      if(pc==0x0801C840 && a==camera+0x14) v+=pad_x;
      if(pc==0x0801C848 && a==camera+0x10) v-=pad_y;
      if(pc==0x0801C84E && a==camera+0x18) v+=pad_y;
    }
    return v;
  }
  void write8(uint32_t a,uint8_t v) override {
    if(a>=0x02000000 && a<0x02040000) { ew[a-0x02000000]=v;return; }
    if(a>=0x03000000 && a<0x03008000) { iw[a-0x03000000]=v;return; }
    oob=true;
  }
  void write16(uint32_t a,uint16_t v) override {write8(a,v);write8(a+1,v>>8);}
  void write32(uint32_t a,uint32_t v) override {write16(a,v);write16(a+2,v>>16);}
};
void require(bool ok,const char* msg) {if(!ok) throw std::runtime_error(msg);}
void cmp(armv4t::CPUState& c,uint32_t a,uint32_t b) {
  uint32_t v=a-b;c.cpsr.n=bool(v>>31);c.cpsr.z=v==0;c.cpsr.c=a>=b;
  c.cpsr.v=bool(((a^b)&(a^v))>>31);
}
uint32_t run(Bus& b,uint32_t entry,uint32_t r0,uint32_t r1=0) {
  armv4t::CPUState c{};c.cpsr.mode=uint8_t(armv4t::Mode::System);
  c.cpsr.t=true;c.thumb=true;c.R[0]=r0;c.R[1]=r1;
  c.R[13]=0x03007E00;c.R[14]=sentinel|1;c.R[15]=entry;
  for(unsigned n=0;n<20000;n++) {
    if(c.R[15]==sentinel) {require(!b.oob,"out-of-range bus access");return c.R[0];}
    b.pc=c.R[15];
    if(b.wide && (b.pc==0x0801C8C8 || b.pc==0x0801C8D0)) {
      cmp(c,c.R[0],b.pc==0x0801C8C8?262u:c.R[1]-22u);
      c.R[15]+=2;continue;
    }
    auto ins=c.thumb?armv4t::ThumbDecoder::decode(b.read16(b.pc),b.pc)
                    :armv4t::ArmDecoder::decode(b.raw32(b.pc),b.pc);
    auto out=armv4t::Interpreter::step(c,b,ins);
    require(out!=armv4t::Interpreter::Result::Undefined &&
            out!=armv4t::Interpreter::Result::NotImplemented &&
            out!=armv4t::Interpreter::Result::Swi,"unsupported original instruction");
  }
  throw std::runtime_error("original routine did not return");
}
int64_t signed32(uint32_t x) {return int32_t(x);}
void setup_actor(Bus& b,int x,int y) {
  std::fill(b.ew.begin()+actor-0x02000000,b.ew.begin()+actor-0x02000000+0x44,0);
  b.write32(actor+0x10,1);b.write32(actor+0x14,resource);
  b.write32(actor+0x28,8);b.write32(actor+0x2C,8);
  b.write32(actor+0x30,x);b.write32(actor+0x34,y);
  b.write32(resource+0x10,0x4000);
}
int64_t ceil_ratio(int64_t n,int64_t d) {return (n+d-1)/d;}
}
int main(int argc,char**argv) {
  try {
    require(argc==2,"usage: test_alfp_projected ROM");
    Bus b;std::ifstream f(argv[1],std::ios::binary);
    require(bool(f),"cannot open ROM");
    b.rom.assign(std::istreambuf_iterator<char>(f),{});
    require(b.rom.size()==0x800000,"ROM size");
    require(std::string(reinterpret_cast<char*>(b.rom.data()+0xAC),4)=="ALFP","ROM game code");
    require(b.read16(0x0801C83A)==0x6803 && b.read16(0x0801C840)==0x6883 &&
            b.read16(0x0801C848)==0x6843 && b.read16(0x0801C84E)==0x68C0 &&
            b.read16(0x0801C918)==0xB4F0,"ROM instruction identity");
    legacy2::ProjectedWorldMargins invalid{99,99};
    require(!legacy2::projected_world_margins(0,0,0,0,22,&invalid)&&!invalid.x&&!invalid.y,
            "uninitialized camera was accepted");
    require(!legacy2::projected_world_margins(1025,0,0,1024,22,&invalid),
            "nonrotation camera was accepted");
    require(!legacy2::projected_world_margins(1024,0,0,1024,22,nullptr),"null output accepted");
    require(legacy2::projected_world_margins(0,0,0,0,0,&invalid)&&!invalid.x&&!invalid.y,
            "zero-margin native mode must preserve bounds");
    require(legacy2::extend_projected_bound(0x80000000u,22,false)==0x80000000u&&
            legacy2::extend_projected_bound(0x7FFFFFFFu,22,true)==0x7FFFFFFFu,
            "signed extreme bound must saturate");
    unsigned cases=0,retained=0,added_left=0,added_right=0,draws=0;
    const int targets[]={-35,-22,-10,0,1,64,120,239,240,250,262,274};
    for(int angle=0;angle<256;angle+=8) {
      b.write32(camera,angle<<10);b.write32(camera+4,1024<<10);b.write32(camera+8,1024<<10);
      b.wide=false;run(b,0x08004BC4,camera);
      const auto camera_before=std::vector<uint8_t>(b.iw.begin()+0xF30,b.iw.begin()+0xF30+0x88);
      int64_t a=signed32(b.raw32(camera+0x38)),bb=signed32(b.raw32(camera+0x3C));
      int64_t c=signed32(b.raw32(camera+0x40)),d=signed32(b.raw32(camera+0x44));
      int64_t tx=signed32(b.raw32(camera+0x48)),ty=signed32(b.raw32(camera+0x4C));
      int64_t det=a*d-bb*c;
      require(det>1000000 && det<1100000,"unexpected camera determinant");
      const int reference_pad_x=int(ceil_ratio(22*1024*std::abs(d),det)+1);
      const int reference_pad_y=int(ceil_ratio(22*1024*std::abs(bb),det)+1);
      legacy2::ProjectedWorldMargins production{};
      require(legacy2::projected_world_margins(int32_t(a),int32_t(bb),int32_t(c),int32_t(d),22,&production),
              "production helper rejected original ROM matrix");
      require(production.x==uint32_t(reference_pad_x)&&production.y==uint32_t(reference_pad_y),
              "production inverse margin differs from independent 64-bit reference");
      b.pad_x=int(production.x);b.pad_y=int(production.y);
      for(int depth=-200;depth<=45;depth+=7) for(int sx:targets) {
        int64_t scale=1024-((3710LL*depth+742000)>>10);
        int64_t X=(sx-120+8)*scale,Y=depth*1024;
        int x=int((d*(X-tx)-c*(Y-ty))/det),y=int((-bb*(X-tx)+a*(Y-ty))/det);
        setup_actor(b,x,y);b.wide=false;
        uint32_t native=run(b,0x0801C822,actor);
        setup_actor(b,x,y);b.wide=true;
        uint32_t wide=run(b,0x0801C822,actor);
        ++cases;
        require(!native||wide,"widening removed a native-visible actor");
        retained+=native!=0;
        int actual_x=int32_t(b.raw32(actor+0x38));
        if(wide&&!native && actual_x<0) ++added_left;
        if(wide&&!native && actual_x>240) ++added_right;
        require(std::equal(camera_before.begin(),camera_before.end(),b.iw.begin()+0xF30),
                "camera state was changed");
        require(b.raw32(actor+0x30)==uint32_t(x)&&b.raw32(actor+0x34)==uint32_t(y),
                "actor world position was changed");
        if(wide) {
          b.write32(context,oam);b.write32(context+4,affine);b.write32(context+8,0);
          uint32_t scale_before=b.raw32(actor+0x40),y_before=b.raw32(actor+0x3C);
          run(b,0x0801C918,actor,context);
          require(((b.raw32(oam)>>16)&511)==(uint32_t(actual_x)&511),"OAM x packing");
          require((b.raw32(oam)&255)==(y_before&255),"OAM y packing");
          require(b.read16(affine+6)==uint16_t(scale_before)&&
                  b.read16(affine+0x1E)==uint16_t(scale_before),"affine scale packing");
          require(b.raw32(context)==oam+8&&b.raw32(context+4)==affine+32&&
                  b.raw32(context+8)==1,"OAM cursor advancement");
          ++draws;
        }
      }
    }
    // Original renderer rejects its sixteenth affine slot without touching cursors/OAM.
    b.write32(context,oam);b.write32(context+4,affine);b.write32(context+8,16);
    const auto before_full=b.ew;
    require(run(b,0x0801C918,actor,context)==0&&before_full==b.ew,
            "full affine OAM context was modified");
    require(added_left&&added_right&&retained&&draws,"missing edge coverage");
    std::printf("ALFP original-ROM projected routines: cases=%u retained=%u added_left=%u added_right=%u OAM_draws=%u; camera/world unchanged; isolated geometry only\n",
                cases,retained,added_left,added_right,draws);
    return 0;
  } catch(const std::exception&e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
