#include "fleet_shell.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cwchar>
int wmain(int argc,wchar_t** argv) {
    if(argc!=3)return 2;
    const bool expected=wcscmp(argv[2],L"valid")==0;
    const bool configured=ConfigureFleetShells(argv[1]);
    ShellProjection p{},ray{};
    for(int i=0;i<3;++i) {
        const bool nearest=ProjectFleet(1,{0,0,0},p);
        const bool incoming=ProjectFleetIncoming(1,{0,0,-1},{0,0,-1},ray);
        if(nearest!=expected||incoming!=expected)return 3;
        if(expected&&(!std::isfinite(p.distance)||!std::isfinite(ray.distance)||p.code>=2097152||ray.code>=2097152))return 4;
    }
    const auto stats=FleetCacheStatistics();
    if(configured&&stats.attempts!=1)return 5;
    if(configured&&!expected&&stats.failures!=1)return 6;
    printf("PASS: configured=%d projection=%d loads=%llu failures=%llu cache=%llu bytes\n",configured,expected,stats.attempts,stats.failures,stats.bytes);
    return 0;
}
