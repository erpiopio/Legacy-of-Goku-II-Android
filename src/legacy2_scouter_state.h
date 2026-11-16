#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>

namespace legacy2::scouter {
// [valid, region, area, actorX8, actorY8, width, height, markerX,
// markerY, reveal, generation, 128 bytes of visited flags].
using Snapshot = std::array<std::int32_t, 139>;
inline std::uint32_t word(const std::uint8_t* p) {
    return p[0] | std::uint32_t(p[1])<<8 | std::uint32_t(p[2])<<16 | std::uint32_t(p[3])<<24;
}
inline std::int32_t project(int start, int end, int fixed, int dimension) {
    return start*8 + static_cast<std::int32_t>(
        (std::int64_t(end-start)*8*(fixed>>8)+(dimension>>1))/dimension);
}
inline Snapshot decode(const std::uint8_t* iw, std::size_t ni,
        const std::uint8_t* ew, std::size_t ne,
        const std::uint8_t* rom, std::size_t nr, unsigned generation) {
    Snapshot out{};out[1]=-1;out[10]=generation;
    // The imported file is hash-verified by setup. Runtime ROM storage may
    // have its header patched/padded; identify the immutable ALFP map table.
    if(!iw||ni<0x1FAC||!ew||!rom||nr<8388608||
            word(rom+0x303AF4)!=68||word(rom+0x303AF8)!=0x083039E4)return out;
    auto ram=[&](std::uint32_t a,std::size_t n)->const std::uint8_t* {
        if(a<0x02000000||a-0x02000000>ne||n>ne-(a-0x02000000))return nullptr;
        return ew+a-0x02000000;
    };
    const unsigned area=iw[0xF0C]|unsigned(iw[0xF0D])<<8;
    out[2]=area;
    out[5]=static_cast<int32_t>(word(iw+0xF84));
    out[6]=static_cast<int32_t>(word(iw+0xF88));
    out[9]=(iw[0x10AD]&8)!=0;
    for(unsigned i=0;i<128;++i)out[11+i]=iw[0x1193+i];
    if(out[5]<=0||out[6]<=0||out[5]>32768||out[6]>32768)return out;
    const auto query=word(iw+0x1FA8);
    // SCOUTER_AUTONOMO: localizar al jugador sin abrir SELECT + R.
    const std::uint8_t* actor=nullptr;

    auto is_player = [&](const std::uint8_t* a) -> bool {
        if(!a)return false;

        const auto v=word(a);
        if(v<0x08000000u || v-0x08000000u>nr-0x28u)
            return false;

        if(v+word(rom+v-0x08000000u+0x24u)!=0x08015A31u)
            return false;

        const auto x=static_cast<int32_t>(word(a+0x158));
        const auto y=static_cast<int32_t>(word(a+0x15C));

        return x>=0 && y>=0 &&
            static_cast<std::int64_t>(x)<=
                static_cast<std::int64_t>(out[5])*256 &&
            static_cast<std::int64_t>(y)<=
                static_cast<std::int64_t>(out[6])*256;
    };

    // Primero intentar las listas originales del motor.
    for(unsigned list=0;list<2&&!actor;++list) {
        auto cp=ram(query+(list?0x40Cu:4u),4);
        if(!cp)continue;

        unsigned count=word(cp);
        if(count>256)continue;

        auto items=ram(query+(list?0x410u:8u),count*4u);
        if(!items)continue;

        for(unsigned i=count;i>0;--i) {
            auto candidate=ram(word(items+(i-1)*4u),0x163u);
            if(is_player(candidate)) {
                actor=candidate;
                break;
            }
        }
    }

    // Búsqueda autónoma en EWRAM.
    // La dirección se conserva y se valida cada fotograma.
    static std::size_t cached_offset=static_cast<std::size_t>(-1);

    if(!actor && cached_offset<ne &&
            0x163u<=ne-cached_offset) {
        auto candidate=ew+cached_offset;
        if(is_player(candidate))
            actor=candidate;
        else
            cached_offset=static_cast<std::size_t>(-1);
    }

    // Si la dirección anterior no sirve, buscar al jugador real.
    if(!actor) {
        const std::uint8_t* encontrado=nullptr;
        std::size_t posicion=0;
        unsigned encontrados=0;

        for(std::size_t o=0;o+0x163u<=ne;o+=4u) {
            auto candidate=ew+o;
            if(!is_player(candidate))continue;

            ++encontrados;
            if(encontrados>1)break;

            encontrado=candidate;
            posicion=o;
        }

        // Evitar elegir un personaje ambiguo.
        if(encontrados==1) {
            actor=encontrado;
            cached_offset=posicion;
        }
    }

    if(!actor)return out;
    out[3]=static_cast<int32_t>(word(actor+0x158));
    out[4]=static_cast<int32_t>(word(actor+0x15C));
    for(unsigned i=0;i<68;++i) {
        auto ga=word(rom+0x3039E4+i*4);
        if(ga<0x08000000||ga-0x08000000>nr-24)return out;
        auto g=ga-0x08000000;auto count=word(rom+g+20);
        if(count>512||g+24+count*4>nr)return out;
        for(unsigned j=0;j<count;++j) {
            auto pa=word(rom+g+24+j*4);
            if(pa<0x08000000||pa-0x08000000>nr-36)return out;
            auto p=rom+pa-0x08000000;
            if((p[0]|unsigned(p[1])<<8)!=area)continue;
            int b[4];for(unsigned k=0;k<4;++k)b[k]=static_cast<int32_t>(word(p+4+k*4));
            if(b[2]<b[0]||b[3]<b[1])return out;
            out[1]=i;out[7]=project(b[0],b[2],out[3],out[5]);
            out[8]=project(b[1],b[3],out[4],out[6]);out[0]=1;return out;
        }
    }
    return out;
}
}
