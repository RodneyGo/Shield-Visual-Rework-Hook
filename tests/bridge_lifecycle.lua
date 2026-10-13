-- Uses the real bridge with a synthetic engine and native API.
SVRNativeShips={};SVRShieldClasses={}
local callbacks,starts,stops,polls,discoveries={},0,0,0,0
local present,now=true,100
local startResult='SVR: ACTIVE; contact=on missile=on'
local allowFailure=false
local notes={}
local api={status=function()return 'ready'end,note=function(s)notes[#notes+1]=s end,
 start=function()starts=starts+1;return startResult end,
 stop=function()stops=stops+1;return 'paused'end,
 poll=function()polls=polls+1;return 'active'end,marker=function()return nil end}
package.loadlib=function()return function()return api end end
GetUISafeModeOption=function()return false end
DebugError=function(s)assert(allowFailure or not s:find('stopped',1,true),s)end
require=function(name)assert(name=='ffi');return {cdef=function()end,C={IsPlayerValid=function()return present end}}end
RegisterEvent=function(name,fn)callbacks[name]=fn end
SetScript=function(_,fn)callbacks.update=fn end
getElapsedTime=function()return now end
AddUITriggeredEvent=function(_,control)assert(control=='register_addons');discoveries=discoveries+1 end
dofile(PROBE_BOOTSTRAP)
callbacks.update();assert(starts==1 and polls==1 and stops==0)
present=false;callbacks.update();assert(stops==1 and polls==1)
callbacks.update();assert(stops==1 and polls==1)
present=true;now=200;callbacks.update();assert(starts==2 and polls==2)
now=50;callbacks.update();assert(stops==2 and starts==3 and polls==3)
assert(discoveries==3)
print('PASS: menu/load, repeated menu, new session, backward clock and addon rediscovery')
for _,mode in ipairs({'nil','rejected'}) do
    allowFailure=true
    startResult=mode=='rejected' and 'SVR: hooks failed; disabled' or nil
    starts,stops,polls,notes=0,0,0,{}
    dofile(PROBE_BOOTSTRAP)
    callbacks.update()
    assert(starts==1 and polls==0 and stops==1)
    assert(notes[#notes]:find('native start rejected',1,true))
    callbacks.update()
    assert(starts==1 and polls==0 and stops==1)
end
print('PASS: nil/disabled native start never marks bridge active; failure is recorded once')
