-- Plain-text profile storage and non-accumulating component positioning.
local M = { STEP = 0.25, LIMIT = 10 }

function M.vector(v)
    if v == nil then return nil end
    local out = {}
    for _, axis in ipairs({"X", "Y", "Z"}) do
        local n = v[axis]
        if type(n) ~= "number" or n ~= n or math.abs(n) == math.huge then return nil end
        out[axis] = n
    end
    return out
end

function M.same(a, b)
    return a and b and math.abs(a.X-b.X) < 0.0001
        and math.abs(a.Y-b.Y) < 0.0001 and math.abs(a.Z-b.Z) < 0.0001
end

function M.target(previous, current, offset)
    -- If the engine resets/repositions this component, adopt its new baseline.
    -- Otherwise retain the baseline from before our last write, never add twice.
    local base = previous and M.same(current, previous.applied) and previous.base or current
    return {base = M.vector(base), applied = {
        X = base.X + offset.X, Y = base.Y + offset.Y, Z = base.Z + offset.Z,
    }}
end

function M.decode(text)
    local profiles = {}
    for line in text:gmatch("[^\r\n]+") do
        if not line:match("^%s*[#;]") and not line:match("^%s*$") then
            local key, x, y, z = line:match("^([%w_]+|%a+|[%w_]+)=([^,]+),([^,]+),([^,]+)$")
            local sex = key and key:match("|(%a+)|")
            local v = M.vector({X=tonumber(x), Y=tonumber(y), Z=tonumber(z)})
            if not key or (sex ~= "male" and sex ~= "female") or not v
                or math.max(math.abs(v.X), math.abs(v.Y), math.abs(v.Z)) > M.LIMIT
                or profiles[key] then
                return nil, "invalid or duplicate offset profile: " .. line
            end
            profiles[key] = v
        end
    end
    return profiles
end

function M.encode(profiles)
    local keys, lines = {}, {"# Horns for All: race|sex|style=X,Y,Z (parent-local Unreal units)"}
    for key in pairs(profiles) do keys[#keys+1] = key end
    table.sort(keys)
    for _, key in ipairs(keys) do
        local v = profiles[key]
        lines[#lines+1] = string.format("%s=%.4f,%.4f,%.4f", key, v.X, v.Y, v.Z)
    end
    return table.concat(lines, "\n") .. "\n"
end

function M.load(path)
    local f, err, code = io.open(path, "r")
    if not f then
        if code == 2 then return {} end -- no saved corrections yet
        return nil, err
    end
    local contents, readError = f:read("*a")
    f:close()
    if not contents then return nil, readError end
    return M.decode(contents)
end

function M.save(path, profiles)
    local text = M.encode(profiles)
    local valid, err = M.decode(text)
    if not valid then return nil, err end
    local f
    f, err = io.open(path .. ".tmp", "w")
    if not f then return nil, err end
    local written, writeError = f:write(text)
    local closed, closeError = f:close()
    if not written or not closed then return nil, writeError or closeError end
    local old, openError, code = io.open(path, "r")
    if not old and code ~= 2 then return nil, openError end
    if old then
        old:close()
        os.remove(path .. ".bak")
        local moved
        moved, err = os.rename(path, path .. ".bak")
        if not moved then return nil, err end
    end
    local moved
    moved, err = os.rename(path .. ".tmp", path)
    if not moved then
        if old then
            local restored, restoreError = os.rename(path .. ".bak", path)
            if not restored then err = tostring(err) .. "; restore failed: " .. tostring(restoreError) end
        end
        return nil, err
    end
    return true
end

return M
