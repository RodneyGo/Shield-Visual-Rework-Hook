#include "fleet_shell.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <numeric>
#include <vector>
#include <set>
#include <chrono>

namespace {
#pragma pack(push, 1)
struct Entry { uint32_t id; uint64_t offset; uint32_t vertices, faces; float lo[3], span[3]; uint32_t quanta[3]; };
#pragma pack(pop)
static_assert(sizeof(Entry)==56);
static_assert(sizeof(Vec3)==12);
struct Node { Vec3 lo, hi; unsigned begin, count, left, right; };
static_assert(sizeof(Node)==40);
struct Mesh {
    Entry meta{};
    std::vector<Vec3> vertices;
    std::vector<std::array<uint32_t,3>> faces;
    std::vector<unsigned> order;
    std::vector<Node> nodes;
};
struct SourceEntry { Entry meta; std::wstring path; bool prebuilt; uint64_t bytes; };
std::map<unsigned, SourceEntry> entries;
std::map<unsigned, Mesh> loaded;
std::set<unsigned> failedLoads;
size_t loadedBytes=0;
uint64_t loadAttempts=0,loadFailures=0,lastLoadMicroseconds=0;
constexpr size_t CacheBudget=256ull*1024*1024;
constexpr unsigned MaxVertices=65536,MaxFaces=131072;
constexpr float CoordinateLimit=1000000.f;
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
Vec3 Cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float Dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
float Axis(Vec3 a, int i) { return i==0?a.x:i==1?a.y:a.z; }
Vec3 Min(Vec3 a, Vec3 b) { return {std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)}; }
Vec3 Max(Vec3 a, Vec3 b) { return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)}; }
bool Finite(Vec3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
bool Bounded(Vec3 a) {return Finite(a)&&std::abs(a.x)<=CoordinateLimit&&std::abs(a.y)<=CoordinateLimit&&std::abs(a.z)<=CoordinateLimit;}
#ifdef SVR_BVH_BUILDER
Vec3 Centre(const Mesh& m,unsigned face) { auto& f=m.faces[face];return (m.vertices[f[0]]+m.vertices[f[1]]+m.vertices[f[2]])*(1.f/3.f); }
unsigned Build(Mesh& m, unsigned begin, unsigned count) {
    Node n{{1e30f,1e30f,1e30f},{-1e30f,-1e30f,-1e30f},begin,count,0,0};
    for(unsigned i=begin;i<begin+count;++i) for(unsigned k:m.faces[m.order[i]]) {
        n.lo=Min(n.lo,m.vertices[k]);n.hi=Max(n.hi,m.vertices[k]);
    }
    unsigned index=static_cast<unsigned>(m.nodes.size());m.nodes.push_back(n);
    if(count>8) {
        Vec3 size=n.hi-n.lo;int axis=size.x>size.y?0:1;if(size.z>Axis(size,axis))axis=2;
        unsigned mid=begin+count/2;
        std::nth_element(m.order.begin()+begin,m.order.begin()+mid,m.order.begin()+begin+count,
                         [&](unsigned a,unsigned b){return Axis(Centre(m,a),axis)<Axis(Centre(m,b),axis);});
        const auto left=Build(m,begin,count/2),right=Build(m,mid,count-count/2);
        m.nodes[index].left=left;m.nodes[index].right=right;m.nodes[index].count=0;
    }
    return index;
}
#endif
bool Contains(Vec3 lo,Vec3 hi,Vec3 p) {
    return p.x>=lo.x&&p.y>=lo.y&&p.z>=lo.z&&p.x<=hi.x&&p.y<=hi.y&&p.z<=hi.z;
}
bool ValidateTree(const Mesh& m) {
    // Forward-only children and one parent per node guarantee an acyclic tree.
    // Verify coverage and boxes so damaged caches cannot silently miss contacts.
    std::vector<unsigned char> visited(m.nodes.size(),0), covered(m.faces.size(),0), seen(m.faces.size(),0);
    std::vector<unsigned> depth(m.nodes.size(),0);visited[0]=1;
    for(auto face:m.order) { if(face>=seen.size()||seen[face]++)return false; }
    for(unsigned i=0;i<m.nodes.size();++i) {
        const auto& n=m.nodes[i];
        if(!visited[i]||!Finite(n.lo)||!Finite(n.hi)||!Contains(n.lo,n.hi,n.hi)||depth[i]>64)return false;
        if(n.count) {
            if(n.count>8||n.begin>m.order.size()||n.count>m.order.size()-n.begin)return false;
            for(unsigned j=n.begin;j<n.begin+n.count;++j) {
                if(covered[j]++)return false;
                for(auto v:m.faces[m.order[j]])if(!Contains(n.lo,n.hi,m.vertices[v]))return false;
            }
        } else {
            for(unsigned child:{n.left,n.right}) {
                if(child<=i||child>=m.nodes.size()||visited[child]++)return false;
                const auto& c=m.nodes[child];
                if(!Contains(n.lo,n.hi,c.lo)||!Contains(n.lo,n.hi,c.hi))return false;
                depth[child]=depth[i]+1;
            }
        }
    }
    return std::all_of(covered.begin(),covered.end(),[](auto x){return x==1;});
}
Vec3 Nearest(const Mesh& m, Vec3 p, unsigned face) {
    auto& f=m.faces[face];Vec3 a=m.vertices[f[0]],b=m.vertices[f[1]],c=m.vertices[f[2]];
    Vec3 ab=b-a,ac=c-a,ap=p-a;
    const Vec3 area=Cross(ab,ac);
    if(Dot(area,area)<1e-20f) {
        auto segment=[&](Vec3 v,Vec3 w){Vec3 d=w-v;float l=Dot(d,d);return l>1e-20f?v+d*std::clamp(Dot(p-v,d)/l,0.f,1.f):v;};
        Vec3 best=segment(a,b);for(auto q:{segment(b,c),segment(c,a)})if(Dot(p-q,p-q)<Dot(p-best,p-best))best=q;
        return best;
    }
    float d1=Dot(ab,ap),d2=Dot(ac,ap);if(d1<=0&&d2<=0)return a;
    Vec3 bp=p-b;float d3=Dot(ab,bp),d4=Dot(ac,bp);if(d3>=0&&d4<=d3)return b;
    float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return a+ab*(d1/(d1-d3));
    Vec3 cp=p-c;float d5=Dot(ab,cp),d6=Dot(ac,cp);if(d6>=0&&d5<=d6)return c;
    float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return a+ac*(d2/(d2-d6));
    float va=d3*d6-d5*d4;if(va<=0&&d4-d3>=0&&d5-d6>=0)return b+(c-b)*((d4-d3)/((d4-d3)+(d5-d6)));
    float sum=va+vb+vc;if(std::abs(sum)<1e-20f)return a;
    return a+ab*(vb/sum)+ac*(vc/sum);
}
void Triangle(const Mesh& m, Vec3 p, unsigned face, ShellProjection& r, float& best) {
    ++r.tested;Vec3 q=Nearest(m,p,face),d=p-q;float sq=Dot(d,d);
    if(Finite(q)&&std::isfinite(sq)&&sq<best){best=sq;r.point=q;r.face=face;}
}
float BoxDistance(Vec3 p,const Node& n) {Vec3 d=p-Max(n.lo,Min(n.hi,p));return Dot(d,d);}
void Query(const Mesh& m,unsigned index,Vec3 p,ShellProjection& r,float& best) {
    const auto& n=m.nodes[index];if(BoxDistance(p,n)>best)return;
    if(n.count){for(unsigned j=n.begin;j<n.begin+n.count;++j)Triangle(m,p,m.order[j],r,best);return;}
    unsigned a=n.left,b=n.right;if(BoxDistance(p,m.nodes[a])>BoxDistance(p,m.nodes[b]))std::swap(a,b);
    Query(m,a,p,r,best);Query(m,b,p,r,best);
}
Mesh* Load(unsigned id) {
    if(auto it=loaded.find(id);it!=loaded.end())return &it->second;
    if(failedLoads.count(id))return nullptr;
    auto it=entries.find(id);if(it==entries.end())return nullptr;
    failedLoads.insert(id);
    struct Timer {
        std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
        bool success=false;
        ~Timer(){lastLoadMicroseconds=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();if(!success)++loadFailures;}
    } timer;
    ++loadAttempts;
    const auto& source=it->second;
    const bool prebuilt=source.prebuilt;
    const uint64_t dataSize=source.bytes;
    const size_t upperBytes=12ull*source.meta.vertices+96ull*source.meta.faces;
    if(upperBytes>CacheBudget-loadedBytes)return nullptr;
    FILE* file=nullptr;if(_wfopen_s(&file,source.path.c_str(),L"rb")||!file)return nullptr;
    Mesh m;m.meta=source.meta;
    // Allocation failures must not leak the file or partially publish a mesh.
    try {m.vertices.resize(m.meta.vertices);m.faces.resize(m.meta.faces);}catch(...){fclose(file);throw;}
    bool ok=_fseeki64(file,static_cast<__int64>(m.meta.offset),SEEK_SET)==0 &&
        fread(m.vertices.data(),sizeof(Vec3),m.vertices.size(),file)==m.vertices.size() &&
        fread(m.faces.data(),sizeof(m.faces[0]),m.faces.size(),file)==m.faces.size();
    if(ok&&prebuilt) {
        uint32_t count=0;
        ok=fread(&count,4,1,file)==1&&count>0&&count<=2ull*m.faces.size();
        const uint64_t end=m.meta.offset+12ull*(m.meta.vertices+uint64_t(m.meta.faces))+4ull+4ull*m.meta.faces+40ull*count;
        ok=ok&&end<=dataSize;
        if(ok) {
            try {m.order.resize(m.meta.faces);m.nodes.resize(count);}catch(...){fclose(file);throw;}
            ok=fread(m.order.data(),4,m.order.size(),file)==m.order.size()&&
               fread(m.nodes.data(),sizeof(Node),m.nodes.size(),file)==m.nodes.size();
        }
    }
    fclose(file);if(!ok)return nullptr;
    for(auto v:m.vertices)if(!Bounded(v))return nullptr;
    for(auto f:m.faces)for(unsigned v:f)if(v>=m.vertices.size())return nullptr;
    if(prebuilt) {if(!ValidateTree(m))return nullptr;}
#ifdef SVR_BVH_BUILDER
    else {
        m.order.resize(m.faces.size());std::iota(m.order.begin(),m.order.end(),0);m.nodes.reserve(m.faces.size());
        Build(m,0,static_cast<unsigned>(m.faces.size()));
    }
#else
    else return nullptr;
#endif
    const size_t bytes=12*m.vertices.size()+12*m.faces.size()+4*m.order.size()+40*m.nodes.size();
    auto* result=&loaded.emplace(id,std::move(m)).first->second;
    loadedBytes+=bytes;failedLoads.erase(id);timer.success=true;return result;
}
bool Encode(const Mesh& m,ShellProjection& r) {
    if(!Bounded(r.point)||!std::isfinite(r.distance))return false;
    unsigned q[3];for(int i=0;i<3;++i){
        const double value=(double(Axis(r.point,i))-m.meta.lo[i])/m.meta.span[i]*m.meta.quanta[i]+.5;
        if(!std::isfinite(value))return false;
        q[i]=static_cast<unsigned>(std::clamp(std::floor(value),0.,double(m.meta.quanta[i])));
    }
    r.code=q[0]+(m.meta.quanta[0]+1)*(q[1]+(m.meta.quanta[1]+1)*q[2]);
    return r.code<2097152;
}
bool RayBox(Vec3 p,Vec3 d,const Node& n,float limit) {
    float lo=0,hi=limit;
    for(int i=0;i<3;++i) {
        const float delta=Axis(d,i),v=Axis(p,i),a=Axis(n.lo,i),b=Axis(n.hi,i);
        if(std::abs(delta)<1e-9f) { if(v<a||v>b) return false; continue; }
        float t0=(a-v)/delta,t1=(b-v)/delta;if(t0>t1)std::swap(t0,t1);
        lo=std::max(lo,t0);hi=std::min(hi,t1);if(lo>hi)return false;
    }
    return true;
}
void RayQuery(const Mesh& m,unsigned index,Vec3 p,Vec3 d,ShellProjection& r,float& best,bool& found) {
    const auto& n=m.nodes[index];if(!RayBox(p,d,n,best))return;
    if(!n.count) { RayQuery(m,n.left,p,d,r,best,found);RayQuery(m,n.right,p,d,r,best,found);return; }
    for(unsigned i=n.begin;i<n.begin+n.count;++i) {
        ++r.tested;const unsigned face=m.order[i];const auto& f=m.faces[face];
        Vec3 a=m.vertices[f[0]],e1=m.vertices[f[1]]-a,e2=m.vertices[f[2]]-a,h=Cross(d,e2);
        float det=Dot(e1,h);if(!std::isfinite(det)||std::abs(det)<1e-8f)continue;
        const float inv=1.0f/det;Vec3 s=p-a;float u=Dot(s,h)*inv;if(!std::isfinite(u)||u<0||u>1)continue;
        Vec3 q=Cross(s,e1);float v=Dot(d,q)*inv;if(!std::isfinite(v)||v<0||u+v>1)continue;
        float t=Dot(e2,q)*inv;if(!std::isfinite(t)||t<0||t>best)continue;
        Vec3 point=p+d*t;if(!Bounded(point))continue;
        best=t;r.point=point;r.face=face;found=true;
    }
}
bool Project(unsigned id,Vec3 p,ShellProjection& r,bool brute) {
    r={};if(!Finite(p)||std::abs(p.x)>1000000||std::abs(p.y)>1000000||std::abs(p.z)>1000000)return false;
    auto* m=Load(id);if(!m)return false;
    float best=1e30f;
    if(brute)for(unsigned i=0;i<m->faces.size();++i)Triangle(*m,p,i,r,best);else Query(*m,0,p,r,best);
    r.distance=std::sqrt(best);if(!std::isfinite(r.distance)||r.distance>120)return false;
    return Encode(*m,r);
}
bool ProjectIncoming(unsigned id,Vec3 p,Vec3 incoming,ShellProjection& r) {
    r={};if(!Finite(p)||!Finite(incoming))return false;
    const float length=std::sqrt(Dot(incoming,incoming));if(length<0.5f||length>1.5f)return false;
    // Keep the existing 120 m proximity requirement; ray travel may be longer
    // at grazing angles. Bound traversal to the shell's diagonal plus padding.
    ShellProjection near{};if(!Project(id,p,near,false))return false;
    auto* m=Load(id);if(!m)return false;
    Vec3 span{m->meta.span[0],m->meta.span[1],m->meta.span[2]};float best=std::sqrt(Dot(span,span))+120;
    bool found=false;RayQuery(*m,0,p,incoming*(-1.0f/length),r,best,found);
    if(!found)return false;
    r.distance=best;r.tested+=near.tested;return Encode(*m,r);
}
}
bool ConfigureFleetShells(const std::wstring& path) {
    entries.clear();loaded.clear();failedLoads.clear();loadedBytes=0;
    return AddFleetShells(path);
}
bool AddFleetShells(const std::wstring& path) {
    FILE* file=nullptr;if(_wfopen_s(&file,path.c_str(),L"rb")||!file)return false;
    char magic[8];uint32_t count=0;
    bool ok=fread(magic,1,8,file)==8;
    const bool prebuilt=ok&&!memcmp(magic,"SVR_FL08",8);
#ifdef SVR_BVH_BUILDER
    ok=ok&&(prebuilt||!memcmp(magic,"SVR_FL07",8));
#else
    ok=ok&&prebuilt;
#endif
    ok=ok&&fread(&count,4,1,file)==1&&count>0&&count<=4096;
    std::vector<Entry> table;
    try {if(ok)table.resize(count);}catch(...){fclose(file);return false;}
    ok=ok&&fread(table.data(),sizeof(Entry),count,file)==count;
    ok=ok&&_fseeki64(file,0,SEEK_END)==0;const auto fileSize=_ftelli64(file);fclose(file);
    if(!ok||fileSize<0)return false;
    std::map<unsigned,SourceEntry> checked;
    for(auto e:table) {
        uint64_t length=12ull*(e.vertices+uint64_t(e.faces))+(prebuilt?4ull+4ull*e.faces+sizeof(Node):0);
        if(!e.id||e.id>4096||!e.vertices||e.vertices>MaxVertices||!e.faces||e.faces>MaxFaces||
           e.offset<12+sizeof(Entry)*count||e.offset>uint64_t(fileSize)||length>uint64_t(fileSize)-e.offset)return false;
        uint64_t product=1;
        for(int i=0;i<3;++i){
            if(!std::isfinite(e.lo[i])||std::abs(e.lo[i])>CoordinateLimit||!std::isfinite(e.span[i])||e.span[i]<=0||e.span[i]>2*CoordinateLimit||!e.quanta[i]||e.quanta[i]>2097151)return false;
            product*=uint64_t(e.quanta[i])+1;
        }
        if(product!=2097152||entries.count(e.id)||!checked.emplace(e.id,SourceEntry{e,path,prebuilt,uint64_t(fileSize)}).second)return false;
    }
    // Publish only after validating the complete file. Collisions never replace live profiles.
    entries.merge(checked);return true;
}
bool HasFleetProfile(unsigned id) { return entries.count(id)!=0; }
bool ProjectFleet(unsigned id,Vec3 p,ShellProjection& r) {try{return Project(id,p,r,false);}catch(...){return false;}}
bool ProjectFleetBrute(unsigned id,Vec3 p,ShellProjection& r) {try{return Project(id,p,r,true);}catch(...){return false;}}
bool ProjectFleetIncoming(unsigned id,Vec3 p,Vec3 incoming,ShellProjection& r) {try{return ProjectIncoming(id,p,incoming,r);}catch(...){return false;}}
void ReleaseFleetShells(){loaded.clear();failedLoads.clear();loadedBytes=0;}
unsigned FleetProfileCount(){return static_cast<unsigned>(entries.size());}
FleetCacheStats FleetCacheStatistics(){return {loadAttempts,loadFailures,loadedBytes,lastLoadMicroseconds};}
#ifdef SVR_BVH_BUILDER
bool BakeFleetShells(const std::wstring& input,const std::wstring& output) {
    if(input==output||!ConfigureFleetShells(input))return false;
    FILE* out=nullptr;if(_wfopen_s(&out,output.c_str(),L"wb"))return false;
    uint32_t count=static_cast<uint32_t>(entries.size());
    std::vector<Entry> table;for(const auto& [id,e]:entries)table.push_back(e.meta);
    bool ok=fwrite("SVR_FL08",1,8,out)==8&&fwrite(&count,4,1,out)==1&&fwrite(table.data(),sizeof(Entry),count,out)==count;
    for(auto& e:table) {
        auto* m=Load(e.id);if(!m){ok=false;break;}
        e.offset=static_cast<uint64_t>(_ftelli64(out));uint32_t nodes=static_cast<uint32_t>(m->nodes.size());
        ok=ok&&fwrite(m->vertices.data(),12,m->vertices.size(),out)==m->vertices.size()&&
          fwrite(m->faces.data(),12,m->faces.size(),out)==m->faces.size()&&fwrite(&nodes,4,1,out)==1&&
          fwrite(m->order.data(),4,m->order.size(),out)==m->order.size()&&fwrite(m->nodes.data(),40,nodes,out)==nodes;
        ReleaseFleetShells();if(!ok)break;
    }
    ok=ok&&_fseeki64(out,12,SEEK_SET)==0&&fwrite(table.data(),sizeof(Entry),count,out)==count;
    if(fclose(out)!=0)ok=false;
    return ok;
}
#endif
