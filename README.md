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
| `${Camera}` | string | `X=... Y=... Z=... H=... P=... FOV=...` |

### Lua

```lua
local mq = require('mq')
print(mq.TLO.Camera.X(), mq.TLO.Camera.Heading(), mq.TLO.Camera.FOV())
```

## License

GPL v3 — see [LICENSE](LICENSE) or the [GNU website](https://www.gnu.org/licenses/).

For build instructions see [BUILDING.md](BUILDING.md).
