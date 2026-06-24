<div align="center">

<img src="https://github.com/Ralzify/MacroManager/blob/main/Macro/app.png" width="96" alt="Macro Manager Icon">

# Macro Manager

**A fast, native keyboard & mouse macro recorder/player for Windows.**

Record what you do, bind it to a key, and play it back with per-macro hotkeys, looping, live recording, and one-click import of other popular Macro Recording apps

</div>

---

## ✨ Highlights

- **🎬 Live recording** — capture keystrokes, mouse clicks, mouse movement, and scroll in real time. Fine-tune what gets recorded (key presses, mouse moves, clicks, delays) and the minimum delay / sampling interval.
- **⌨️ Per-macro hotkeys** — bind any key to trigger a macro. A low-level keyboard hook fires your macros system-wide, even when the app isn't focused.
- **🔁 Looping** — run a macro once or repeat it a set number of times.
- **🖱️ Full input vocabulary** — key press / down / up, absolute & relative mouse moves, clicks, button hold/release, scroll wheel, and timed delays.
- **✏️ Manual editing** — build or tweak macros action-by-action without recording.
- **🔔 Toggle chimes** — optional enable/disable sound cues when you toggle a macro on or off via its hotkey.
- **💾 Automatic persistence** — macros are saved to JSON and reloaded on startup. Import/export profiles to share or back them up.
- **📥 MacroGamer import** — bring your existing `.mgp` profiles straight into Macro Manager.
- **🎨 Clean native UI** — a lightweight Dear ImGui interface rendered with DirectX 11. No Electron, no browser, no bloat.

---

## 📸 Screenshot

<div align="center">
<img src="https://cdn.discordapp.com/attachments/1457829290497409147/1519377444929278085/image.png?ex=6a3d5603&is=6a3c0483&hm=8b15d34f5cffd06da86f4b03ae28f91facdd94fedbca6bcdf0f880c0a8f0914c&" width="640" alt="Macro Manager Screenshot">
</div>

---

## 🚀 Installation

### Option A — Download a release

1. Grab the latest `Macro Manager.exe` from the [Releases](https://github.com/Ralzify/MacroManager/releases) page.
2. Keep `enabled.mp3` and `disabled.mp3` next to the executable if you want toggle chimes.
3. Run it. No installer, no dependencies — it's a single native Windows app.

> **Note:** Because Macro Manager installs a low-level keyboard hook, some antivirus tools or games with anti-cheat may flag or block macro tools. Use it responsibly and only where it's allowed.

### Option B — Build from source:

**Requirements**
- Windows 10 / 11
- A C++17 compiler (MSVC / Visual Studio 2019+ recommended)
- [CMake](https://cmake.org/) 3.16+

**Build with CMake**

```bash
git clone https://github.com/Ralzify/MacroManager.git
cd MacroManager
cmake -B build -A x64
cmake --build build --config Release
```

The executable is produced in `build/Release/`, with the chime sounds copied alongside it automatically.

**OR build with Visual Studio**

Open `MacroManager.vcxproj` in Visual Studio, select the **Release / x64** configuration, and build.

---

## 🕹️ Usage

1. **Create a macro** — click **+ New Macro** and give it a name.
2. **Record** — open **Record Options** to choose what to capture, then hit **Start Recording**. Perform your actions and stop when done; the captured steps are appended to the selected macro.
   - You can also set a global **toggle key** to start/stop recording hands-free.
3. **Edit** — add, remove, or adjust individual actions (keys, mouse moves, clicks, delays) in the editor.
4. **Bind a hotkey** — assign a trigger key so the macro plays on demand, anywhere in Windows.
5. **Loop** — enable repeat and set a count to run the macro multiple times.
6. **Run** — press the macro's hotkey, or use **Run Now**. Hit **Stop All** to halt everything immediately.

Macros are saved automatically to:

```
%APPDATA%\Macro Manager\macros.json
```

### Importing from MacroGamer

Click **Import MGP**, browse to your `.mgp` profile (usually in `Documents\MacroGamer`), and import. Your macros are converted and added to the list.

---

## 🛠️ Tech Stack

- **C++17**
- **[Dear ImGui](https://github.com/ocornut/imgui)** — immediate-mode GUI
- **DirectX 11** + **Win32** — rendering and windowing
- **[nlohmann/json](https://github.com/nlohmann/json)** — macro persistence
- Windows low-level keyboard/mouse hooks, `SendInput`, and WIC for icon loading

---

## 🙏 Credits

- **[Dear ImGui](https://github.com/ocornut/imgui)** by Omar Cornut and contributors — the UI framework.
- **[nlohmann/json](https://github.com/nlohmann/json)** by Niels Lohmann — JSON parsing and serialization.
- Built by **[Ralzify](https://github.com/Ralzify)**.

---

## 📄 License

This project is licensed under a custom attribution license. You can use, modify, and distribute the application, but public forks, releases, installers, and rebrands must keep visible credit to Macro Manager/Ralzify.

---

<div align="center">
<sub>🐼 Made by Ralzify · <a href="https://github.com/Ralzify/MacroManager">github.com/Ralzify/MacroManager</a></sub>
</div>
