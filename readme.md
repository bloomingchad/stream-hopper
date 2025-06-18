# 🎵 stream hopper

a terminal-based radio station switcher for discovering new music through live streams. hop between stations, automatically log song titles, and use those discoveries to build better playlists elsewhere.

**the idea:** human-curated radio beats algorithmic recommendations. find tracks you'd never discover otherwise.

## ✨ key features

- **⚡ instant station switching** - seamlessly hop between radio stations with smooth audio fading
- **🎤 live "now playing" display** - see exactly what's playing on each station in real-time  
- **🔇 smart mute controls** - mute/unmute stations without losing your place
- **🤖 auto-hop mode** - auto-discovery mode that cycles through all stations automatically
- **📝 json history logging** - automatically captures and saves song titles for later reference
- **🎛️ multi-stream management** - monitor multiple stations simultaneously with individual volume control
- **🎨 clean terminal ui** - distraction-free interface built with ncurses

## 🛠️ dependencies

you'll need a c++ compiler and the following development libraries:

### required tools
- `g++` (c++17 or later)
- `make`
- `pkg-config`

### required libraries
- `libmpv-dev` - media playback engine
- `ncurses-dev` - terminal ui framework
- `nlohmann/json` - json parsing (single header file)

### installation commands

**fedora/rhel/centos:**
```bash
sudo dnf install gcc-c++ mpv-devel ncurses-devel pkg-config make
```

**debian/ubuntu:**
```bash
sudo apt update
sudo apt install build-essential libmpv-dev libncurses5-dev pkg-config
```

**arch linux:**
```bash
sudo pacman -s gcc make pkg-config mpv ncurses
```

## 🚀 build & run

1. **download the json header:**
   ```bash
   make
   ```

## 🎮 controls

| key | action |
|-----|--------|
| `↑` / `↓` | switch between radio stations |
| `enter` | mute/unmute current station |
| `a` | toggle auto-hop mode (auto-discovery) |
| `p` | cycle through performance modes |
| `d` | toggle audio ducking |
| `f` | toggle favorite for current station |
| `tab` | switch focus between panels |
| `c` | enter copy mode (pauses UI) |
| `q` | quit application |

### auto-hop mode
press `a` to enter auto-hop mode - the app will automatically cycle through all stations, spending equal time on each one. perfect for passive discovery when you want to sit back and let the music surprise you.

## 📁 configuration

- **station list:** modify the `station_data` vector in `include/StationList.hpp` to add/remove stations
- **song history:** automatically saved to `radio_history.json` in the current directory
- **fade duration:** adjust `FADE_TIME_MS` in `StationManager.cpp` (default: 900ms) for faster/slower transitions
- **auto-hop duration:** modify `AUTO_HOP_TOTAL_TIME_SECONDS` in `RadioPlayer.cpp` (default: ~18 minutes total cycle)

### adding your own stations

edit the `station_data` vector in `include/StationList.hpp`:
```cpp
{"station name", {"https://stream-url-here"}},
```

the app comes pre-configured with a curated selection of european dance, electronic, and german rap stations.

## 📊 history logging

every song title change is automatically logged with timestamps to `radio_history.json`:

```json
{
  "station name": [
    ["2024-06-12 14:30:15", "artist - song title"],
    ["2024-06-12 14:33:42", "another artist - another song"]
  ]
}
```

use this data to build better playlists on spotify, apple music, or your platform of choice!

## 🎯 workflow integration

1. **discover:** run stream hopper and hop between stations until you hear something amazing
2. **capture:** the song title is automatically logged to your history file  
3. **amplify:** search for those tracks on your streaming service to seed new, diverse playlists
4. **repeat:** break the algorithm's hold on your musical taste, one discovery at a time

## 🔧 technical details

- **architecture:** multi-threaded design with separate audio handling and ui threads
- **audio engine:** powered by libmpv for reliable stream playback
- **memory safe:** modern c++ with raii and smart pointers
- **cross-platform:** works on any linux distribution with the required dependencies

## 📝 license

this project is licensed under the mozilla public license 2.0 (mpl-2.0).

## 🙏 acknowledgements

- **[libmpv](https://mpv.io/)** - robust media playback engine
- **[ncurses](https://invisible-island.net/ncurses/)** - terminal user interface library  
- **[nlohmann/json](https://github.com/nlohmann/json)** - modern c++ json library
- **all the radio stations** - for keeping real, human-curated music alive
- **ai assistance** - gemini 2.5 pro and claude sonnet 4 helped with development and documentation

---

*"the algorithm knows what you liked yesterday. radio knows what you'll love tomorrow."*
