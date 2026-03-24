# MQ2Camera

Exposes EverQuest camera position, angles, and field of view to the MacroQuest TLO system. Targets the **RoF2 EMU** client.

## Installation

1. Download `MQ2Camera.dll` from the [latest release](../../releases/latest)
2. Copy it into your MacroQuest `plugins/` folder
3. Either load it once with `/plugin MQ2Camera` or add it to your `macroquest.ini` to auto-load

## Usage

```
/camera
```
Prints all values to chat.

### TLO Members

| Member | Type | Description |
|--------|------|-------------|
| `${Camera.X}` | float | Camera position X |
| `${Camera.Y}` | float | Camera position Y |
| `${Camera.Z}` | float | Camera position Z |
| `${Camera.Heading}` | float | Degrees, 0=North, clockwise |
| `${Camera.Pitch}` | float | Degrees, positive=up |
| `${Camera.Roll}` | float | Degrees |
| `${Camera.FOV}` | float | Vertical field of view in degrees |
| `${Camera.ScreenW}` | float | Render buffer width in pixels |
| `${Camera.ScreenH}` | float | Render buffer height in pixels |
| `${Camera}` | string | `X=... Y=... Z=... H=... P=... FOV=...` |

### World-to-Screen Projection

Index format is `worldX,worldY,worldZ` (in-game coordinates).

| Member | Type | Description |
|--------|------|-------------|
| `${Camera.Project[x,y,z]}` | string | `"sx,sy"` screen pixel coordinates, or FALSE if behind camera |
| `${Camera.ProjectClamped[x,y,z]}` | string | `"sx,sy"` clamped to nearest screen edge — always returns a value even when off-screen or behind camera |
| `${Camera.ProjectX[x,y,z]}` | float | Screen X pixel, or `-1` if behind camera |
| `${Camera.ProjectY[x,y,z]}` | float | Screen Y pixel, or `-1` if behind camera |
| `${Camera.ProjectVisible[x,y,z]}` | bool | TRUE only if in front of camera AND within screen bounds |

### Lua

```lua
local mq = require('mq')

-- Basic camera state
print(mq.TLO.Camera.X(), mq.TLO.Camera.Heading(), mq.TLO.Camera.FOV())

-- Project a world coordinate to screen
local target = mq.TLO.Target
if target() then
    local wx, wy, wz = target.X(), target.Y(), target.Z()
    if mq.TLO.Camera.ProjectVisible(wx..','..wy..','..wz)() then
        local sx = mq.TLO.Camera.ProjectX(wx..','..wy..','..wz)()
        local sy = mq.TLO.Camera.ProjectY(wx..','..wy..','..wz)()
        print(string.format("Target is at screen %.0f, %.0f", sx, sy))
    else
        -- Off-screen: get the nearest edge pixel instead
        local edge = mq.TLO.Camera.ProjectClamped(wx..','..wy..','..wz)()
        print("Target off-screen, nearest edge: "..edge)
    end
end
```

## License

GPL v3 — see [LICENSE](LICENSE) or the [GNU website](https://www.gnu.org/licenses/).

For build instructions see [BUILDING.md](BUILDING.md).
