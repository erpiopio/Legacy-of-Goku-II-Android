#include "legacy2_scouter_state.h"
#include <vector>
#include <fstream>
#include <iterator>
#include <iostream>
std::vector<std::uint8_t> load(const char* f){std::ifstream s(f,std::ios::binary);return {(std::istreambuf_iterator<char>(s)),{}};}
int main(int argc,char**argv){
 if(argc!=3)return 2;auto rom=load(argv[1]),state=load(argv[2]);
 if(state.size()<52||std::string(reinterpret_cast<char*>(state.data()),4)!="GBAS")return 3;
 for(unsigned o=52;o+8<=state.size();){
  auto n=legacy2::scouter::word(state.data()+o+4);
  if(n>state.size()-o-8)return 4;
  if(std::string(reinterpret_cast<char*>(state.data()+o),4)=="BUS0"){
   if(n<0x48000)return 5;auto ew=state.data()+o+8,iw=ew+0x40000;
   auto s=legacy2::scouter::decode(iw,0x8000,ew,0x40000,rom.data(),rom.size(),1);
   std::cout<<"valid="<<s[0]<<" region="<<s[1]<<" area="<<s[2]<<" actor_fixed="<<s[3]<<","<<s[4]<<" dimensions="<<s[5]<<","<<s[6]<<" marker="<<s[7]<<","<<s[8]<<" reveal="<<s[9]<<'\n';
   return s[0]?0:6;
  }o+=8+n;
 }return 7;
}
