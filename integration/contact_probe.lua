-- Native contacts -> prepared fleet shells, without projectile lifetime tracking.
if package == nil or GetUISafeModeOption() ~= false then
    DebugError("SVR CONTACT: Protected UI Mode is ON; native probe not loaded")
    return
end
local loader, why = package.loadlib(".\\extensions\\shield_visuals_rework\\native\\svr_contact_probe.dll", "luaopen_svr_contact_probe")
if not loader then
    DebugError("SVR CONTACT: DLL load failed: " .. tostring(why))
    return
end
local ok, probe = pcall(loader)
if not ok or type(probe) ~= "table" then
    DebugError("SVR CONTACT: initialization failed: " .. tostring(probe))
    return
end
local ffi = require("ffi")
pcall(ffi.cdef, "bool IsPlayerValid(void);")
pcall(ffi.cdef, [[
    typedef uint64_t UniverseID;
    UniverseID GetContextByClass(UniverseID componentid, const char* classname, bool includeself);
    bool IsComponentClass(UniverseID componentid, const char* classname);
]])
-- Included in the core bootstrap after the native API is loaded.
local registeredAddons = {}
local function registerAddon(_, descriptor)
    if type(descriptor) ~= "string" or #descriptor > 65536 then return end
    local addon, registry = descriptor:match("^([a-z0-9_]+)|(.+)$")
    if not addon or #addon > 80 or registeredAddons[addon] then return end
    local pending, ids = {}, {}
    for row in registry:gmatch("[^;]+") do
        local macro, id, cap = row:match("^([a-z0-9_]+):(%d+):(%d+)$")
        id, cap = tonumber(id), tonumber(cap)
        if not macro or not id or id < 1 or id > 4096 or (cap ~= 32 and cap ~= 64)
           or pending[macro] or SVRNativeShips[macro] then
            probe.note("addon registry rejected: " .. addon)
            return
        end
        pending[macro] = {id=id, cap=cap}
        ids[id] = true
    end
    if next(pending) == nil then return end
    if not probe.register_addon or probe.register_addon(addon) ~= "ready" then
        probe.note("addon geometry rejected: " .. addon)
        return
    end
    for id in pairs(ids) do
        if not probe.has_profile or probe.has_profile(tostring(id)) ~= "ready" then
            probe.note("addon profile missing: " .. addon)
            return
        end
    end
    for macro, profile in pairs(pending) do SVRNativeShips[macro] = profile end
    registeredAddons[addon] = true
    probe.note("addon active: " .. addon)
end
RegisterEvent("svr.register_addon", function(...)
    local ok, reason = pcall(registerAddon, ...)
    if not ok then DebugError("SVR addon registration failed: " .. tostring(reason)) end
end)
local nextAddonDiscovery = 0
local addonDiscoveryCount = 0
local function discoverAddons()
    local now = getElapsedTime()
    if now < nextAddonDiscovery then return end
    AddUITriggeredEvent("SVRNativeProbe", "register_addons", 0)
    addonDiscoveryCount = addonDiscoveryCount + 1
    nextAddonDiscovery = now + (addonDiscoveryCount < 5 and 2 or 30)
end

local lastStatus = nil
local started = false
local failed = false
local ships = {}
local activeUntil = {}
local beamActiveUntil = {}
local beamProjectiles = {
    ["bullet_arg_turret_l_beam_01_mk1_macro"]=true,
    ["bullet_atf_l_ljos_macro"]=true,
    ["bullet_atf_xl_beam_01_mk1_macro"]=true,
    ["bullet_atf_xl_mjolnir_macro"]=true,
    ["bullet_bor_l_beam_01_mk1_macro"]=true,
    ["bullet_bor_turret_l_projector_macro"]=true,
    ["bullet_gen_l_ionproj_macro"]=true,
    ["bullet_gen_l_ionproj_turret_macro"]=true,
    ["bullet_gen_l_posbeam_macro"]=true,
    ["bullet_gen_l_posbeam_turret_macro"]=true,
    ["bullet_gen_l_tribeam_macro"]=true,
    ["bullet_gen_l_tribeam_turret_macro"]=true,
    ["bullet_gen_m_beam_01_mk1_macro"]=true,
    ["bullet_gen_m_beam_01_mk2_macro"]=true,
    ["bullet_gen_m_yacht_beam_01_mk1_macro"]=true,
    ["bullet_gen_s_beam_01_mk1_macro"]=true,
    ["bullet_gen_s_beam_01_mk2_macro"]=true,
    ["bullet_gen_turret_l_beam_01_mk1_macro"]=true,
    ["bullet_gen_turret_l_beam_spl_macro"]=true,
    ["bullet_gen_turret_m_beam_01_mk1_macro"]=true,
    ["bullet_kha_l_beam_02_mk1_macro"]=true,
    ["bullet_kha_m_beam_01_macro"]=true,
    ["bullet_kha_s_beam_01_macro"]=true,
    ["bullet_kha_turret_l_beam_01_mk1_macro"]=true,
    ["bullet_kha_turret_l_beam_01_mk1_scenario_macro"]=true,
    ["bullet_kha_turret_m_beam_01_macro"]=true,
    ["bullet_kha_xl_beam_01_mk1_macro"]=true,
    ["bullet_par_turret_l_beam_01_mk1_macro"]=true,
    ["bullet_spl_turret_l_beam_01_mk1_macro"]=true,
    ["bullet_tel_turret_l_beam_01_mk1_macro"]=true,
    ["bullet_ter_m_beam_01_mk1_macro"]=true,
    ["bullet_ter_m_beam_01_mk2_macro"]=true,
    ["bullet_ter_m_meson_macro"]=true,
    ["bullet_ter_s_beam_01_mk1_macro"]=true,
    ["bullet_ter_s_beam_01_mk2_macro"]=true,
    ["bullet_ter_s_meson_macro"]=true,
    ["bullet_ter_scimitar_beam_l_macro"]=true,
    ["bullet_ter_turret_l_beam_01_mk1_macro"]=true,
    ["bullet_ter_turret_m_beam_01_mk1_macro"]=true,
    ["bullet_ter_turret_m_meson_macro"]=true,
    ["bullet_xen_l_waver_macro"]=true,
    ["bullet_xen_m_beam_01_mk1_macro"]=true,
    ["bullet_xen_turret_l_obliterator_macro"]=true,
    ["bullet_xen_turret_l_obliterator_one_macro"]=true,
    ["bullet_xen_turret_m_beam_01_mk1_macro"]=true,
    ["bullet_xen_turret_xl_beam_01_mk1_macro"]=true,
}

local skipped = {}
local rank = {1,2,3,5,4} -- Stored class 4 is super; class 5 is very_high.
local lastCleanup = 0
local function skip(reason)
    skipped[reason] = (skipped[reason] or 0) + 1
    if skipped[reason] == 1 then probe.note("shell skipped: " .. reason) end
end
local function play(id, rootid, target, rootmacro, bullet, x,y,z,seq, incoming)
    local profile = SVRNativeShips and SVRNativeShips[rootmacro]
    if not profile then return end
    -- Subsystem coordinates cannot be treated as root-local. Await a separately
    -- verified transform instead of drawing waves at the wrong end of the hull.
    if id ~= rootid then skip("subsystem transform pending"); return end
    local tier = SVRResolveClass(bullet)
    if tier==0 then return end -- Explicit shield bypass, distinct from unknown medium.
    local shield = GetComponentData(target, "shield")
    if (shield or 0) <= 0 then skip("no active shield"); return end
    local now = getElapsedTime()
    if now - lastCleanup > 5 then
        lastCleanup = now
        for key, state in pairs(ships) do if now-state.seen>10 then ships[key]=nil end end
    end
    local state = ships[rootid]
    if not state then state={series={},slots={},next=0};ships[rootid]=state end
    state.seen=now
    local cachedProjection
    local function contactProjection()
        if cachedProjection then return unpack(cachedProjection) end
    local projected
    if incoming and probe.fleet_project_incoming then
        local dx,dy,dz,source=incoming:match("^([^|]+)|([^|]+)|([^|]+)|(%d+)$")
        if dx and tonumber(source)>0 then
            projected=probe.fleet_project_incoming(string.format("%d|%.9g|%.9g|%.9g|%.9g|%.9g|%.9g",profile.id,x,y,z,tonumber(dx),tonumber(dy),tonumber(dz)))
        end
    end
    if not projected then projected=probe.fleet_project(string.format("%d|%.9g|%.9g|%.9g",profile.id,x,y,z)) end
    if not projected then skip("outside projection range");return end
    local code,px,py,pz,gap,tests=projected:match("^(%d+)|([^|]+)|([^|]+)|([^|]+)|([^|]+)|(%d+)$")
    if not code then skip("invalid projection result");return end
        cachedProjection={code,px,py,pz,gap,tests}
        return unpack(cachedProjection)
    end
    -- Beam contact refresh is independent of ordinary shield wave cadence.
    -- Actual received contacts only: no stale-point extrapolation between ticks.
    if beamProjectiles[bullet] and profile.id>=1 and profile.id<=4096
       and now>=(state.beam36Until or 0) then
        for i=#activeUntil,1,-1 do if activeUntil[i]<=now then table.remove(activeUntil,i) end end
        for i=#beamActiveUntil,1,-1 do if beamActiveUntil[i]<=now then table.remove(beamActiveUntil,i) end end
        -- Limit overlays to 8 globally; 0.1 s cadence and 0.24 s life allow up to 3 per target.
        -- Reserve one global slot for the ordinary shield wave.
        if #activeUntil<95 and #beamActiveUntil<8 then
            local beamCode=contactProjection()
            if beamCode then
                AddUITriggeredEvent("SVRNativeProbe","shell17",{ConvertStringToLuaID(id),"svr_beam32_"..profile.id,0.5+tonumber(beamCode)/2097152,tonumber(seq)})
                state.beam36Until=now+0.1
                table.insert(beamActiveUntil,now+0.24)
                table.insert(activeUntil,now+0.24)
            end
        end
    end
    -- A continuous super beam reports damage ticks, not separate visual blasts.
    -- Keep one full lifecycle per target, even when the contact sweeps the hull.
    -- Do not restart its reveal or extend its expiry on suppressed ticks.
    if tier==4 and (state.superUntil or 0)>now then
        return
    end
    local series, best = nil, 12*12
    -- Group only nearby contacts. This gate runs before the mesh query.
    for _, s in ipairs(state.series) do
        local d=(x-s.x)^2+(y-s.y)^2+(z-s.z)^2
        if now-s.seen<2 and d<best then series=s;best=d end
    end
    local repeated=0
    if series then
        -- Coalesce duplicate/catch-up events in the same frame, not all light hits
        -- between refraction pulses. Original showcase kept that contact layer.
        if rank[tier]<=rank[series.tier] and now-series.seen<0.05 then return end
        if now-series.seen<1 then repeated=1 end
    end
    -- Generic fleet effects have no separate contact layer; limit full waves to 0.7 s.
    if tier<=2 and series and now-series.wave<0.7 then
        series.seen=now;series.tier=tier;return
    end
    for i=#activeUntil,1,-1 do if activeUntil[i]<=now then table.remove(activeUntil,i) end end
    if #activeUntil>=96 then skip("global visual budget");return end
    local code,px,py,pz,gap,tests=contactProjection()
    if not code then return end
    if not series then
        if #state.series>=64 then table.remove(state.series,1) end
        series={seen=-100,wave=-100,count=0,start=now};table.insert(state.series,series)
    end
    if now-series.seen>=1 then series.start=now;series.count=0 end
    series.count=series.count+1
    local lastSeen=series.seen
    series.x=x;series.y=y;series.z=z;series.seen=now;series.tier=tier
    state.next=state.next%profile.cap+1
    local slot=state.next
    local previous=state.slots[slot]
    if previous and previous.expires>now then skip("ship visual budget");return end
    px,py,pz=tonumber(px),tonumber(py),tonumber(pz)
    local repeatMode=(tier~=4 and now-lastSeen<1) and 1 or 0
    local effect=string.format("svr_native_fleet_%d_%d_%d",profile.id,tier,repeatMode)
    local strength=0.5+tonumber(code)/2097152
    local lifetime
    if tier==1 or tier==2 then lifetime=(repeatMode==1 and 48 or 24)/13
    elseif tier==3 or tier==5 then lifetime=3.27
    else lifetime=5 end
    series.wave=now
    -- Same effect definition, independent engine instance and UV clock.
    AddUITriggeredEvent("SVRNativeProbe","shell17",{ConvertStringToLuaID(id),effect,strength,tonumber(seq)})
    if tier==4 then state.superUntil=now+lifetime end
    state.slots[slot]={effect=effect,expires=now+lifetime}
    table.insert(activeUntil,now+lifetime)
end
local function markers()
    for _ = 1, 32 do
        local record = probe.marker()
        if not record then break end
        local incoming=probe.marker_direction and probe.marker_direction()
        local seq, id, x, y, z, bullet, capturedMacro = string.match(record, "^(%d+)|(%d+)|([^|]+)|([^|]+)|([^|]+)|([^|]*)|([^|]*)$")
        if id then
            local target = ConvertStringTo64Bit(id)
            if ffi.C.IsComponentClass(target, "object") then
                local root = ffi.C.GetContextByClass(target, "ship", true)
                if root ~= 0 then
                    local rootid = tostring(root):gsub("ULL$", "")
                    -- FFI UniverseID is cdata; the Lua API requires its own ID representation.
                    local rootmacro = GetComponentData(ConvertStringTo64Bit(rootid), "macro")
                    play(id,rootid,target,rootmacro,bullet,tonumber(x),tonumber(y),tonumber(z),seq,incoming)

                end
            end
        end
    end
end
local function report(text)
    if text ~= lastStatus then
        lastStatus = text
        DebugError(text)
    end
end
local previousTime = nil
local function tick()
    local valid, present = pcall(ffi.C.IsPlayerValid)
    local now = getElapsedTime()
    -- A backward game clock also catches loading an older save without a menu frame.
    if started and (not valid or not present or (previousTime and now < previousTime)) then
        if probe.stop then report(probe.stop()) end
        started = false
        ships, activeUntil, beamActiveUntil = {}, {}, {}
        nextAddonDiscovery, addonDiscoveryCount = 0, 0
    end
    previousTime = now
    if valid and present and not started then
        started = true
        report(probe.start())
    end
    if not started then return end
    discoverAddons()
    report(probe.poll())
    markers()
end
local function update()
    if failed then return end
    local ok, why = pcall(tick)
    if not ok then
        failed = true
        if probe.stop then probe.stop() end
        report("SVR CONTACT: UI bridge stopped: " .. tostring(why))
        probe.note("UI bridge stopped: " .. tostring(why))
    end
end
report(probe.status())
SetScript("onUpdate", update)
