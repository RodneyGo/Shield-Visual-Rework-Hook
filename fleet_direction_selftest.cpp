#include "fleet_shell.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#pragma pack(push,1)
struct Entry { uint32_t id; uint64_t offset; uint32_t vertices,faces; float lo[3],span[3]; uint32_t quanta[3]; };
#pragma pack(pop)
void Check(bool v,const char* s){if(!v){fprintf(stderr,"FAIL: %s\n",s);exit(1);}}
Vec3 Sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 Cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float Dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 Mul(Vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
float Ray(Vec3 p,Vec3 dir,const std::vector<Vec3>& v,const std::vector<std::array<unsigned,3>>& f){
 float best=1e30f;
 for(auto t:f){auto e1=Sub(v[t[1]],v[t[0]]),e2=Sub(v[t[2]],v[t[0]]),h=Cross(dir,e2);float det=Dot(e1,h);if(fabs(det)<1e-7f)continue;
 auto s=Sub(p,v[t[0]]);float u=Dot(s,h)/det;if(u<0||u>1)continue;auto q=Cross(s,e1);float w=Dot(dir,q)/det;if(w<0||u+w>1)continue;float d=Dot(e2,q)/det;if(d>=0)best=std::min(best,d);}
 return best;
}
int wmain(int argc,wchar_t** argv){
 Check(argc==3||argc==4,"base and addon files, optional expected count");unsigned expectedCount=argc==4?static_cast<unsigned>(_wtoi(argv[3])):322;Check(ConfigureFleetShells(argv[1])&&AddFleetShells(argv[2]),"configure all profiles");unsigned tested=0;
 for(int input=1;input<=2;++input){
  FILE* file=nullptr;Check(!_wfopen_s(&file,argv[input],L"rb"),"open geometry");char magic[8];unsigned count;fread(magic,1,8,file);fread(&count,4,1,file);std::vector<Entry> entries(count);fread(entries.data(),sizeof(Entry),count,file);
  for(auto e:entries){
   std::vector<Vec3> v(e.vertices);std::vector<std::array<unsigned,3>> f(e.faces);_fseeki64(file,e.offset,SEEK_SET);Check(fread(v.data(),sizeof(Vec3),v.size(),file)==v.size()&&fread(f.data(),12,f.size(),file)==f.size(),"read mesh");
   for(unsigned sample=0;sample<3;++sample){auto t=f[(f.size()/4)*(sample+1)];Vec3 centre{(v[t[0]].x+v[t[1]].x+v[t[2]].x)/3,(v[t[0]].y+v[t[1]].y+v[t[2]].y)/3,(v[t[0]].z+v[t[1]].z+v[t[2]].z)/3};auto normal=Cross(Sub(v[t[1]],v[t[0]]),Sub(v[t[2]],v[t[0]]));float len=sqrt(Dot(normal,normal));if(len<1e-6f)continue;
    Vec3 d=Mul(normal,1/len);Vec3 p=Sub(centre,Mul(d,-0.25f));ShellProjection result{};
    Check(ProjectFleetIncoming(e.id,p,d,result),"directional projection");float expected=Ray(p,Mul(d,-1),v,f);Check(fabs(expected-result.distance)<0.01f,"BVH ray matches independent brute ray");
    Check(!ProjectFleetIncoming(e.id,p,{0,0,0},result),"invalid direction rejected");Check(ProjectFleet(e.id,p,result),"nearest fallback available");
   }
   ++tested;ReleaseFleetShells();
  }fclose(file);
 }
 Check(tested==expectedCount,"base and addon coverage");printf("PASS: directional BVH and brute ray comparison plus fallback across %u profiles.\n",tested);
}
