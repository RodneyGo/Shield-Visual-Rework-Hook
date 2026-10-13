#include "fleet_shell.h"
#include <cstdio>
#include <cmath>
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 2;
    bool accepted=ConfigureFleetShells(argv[1]);
    ShellProjection result{};
    bool nearest=accepted && ProjectFleet(1,{0,0,0},result);
    std::printf("accepted=%d nearest=%d distance=%g code=%u\n",accepted,nearest,result.distance,result.code);
    bool ray=accepted && ProjectFleetIncoming(1,{0,0,0},{0,0,-1},result);
    std::printf("ray=%d finite_point=%d distance=%g point=%g,%g,%g\n",ray,std::isfinite(result.point.x)&&std::isfinite(result.point.y)&&std::isfinite(result.point.z),result.distance,result.point.x,result.point.y,result.point.z);
}
