# Deskplorer

A persistent, dockable panel that keeps Windows Explorer (and your desktop) always visible on screen — no more Alt-Tabbing to check a folder or drag a file.

## Idea

I wanted access to my Desktop, and to Windows Explorer in general, without ever switching between windows for it. Instead of alt-tabbing to a separate Explorer window every time I needed to browse or drag a file, I wanted a real Explorer view docked to the edge of the screen, always there, always available — like a permanent sidebar rather than a window you have to bring to front.

Deskplorer reserves a thin vertical strip on the screen (like a taskbar or an AppBar) and hosts a real Windows Explorer view inside it, with tabs, so browsing files feels native but never takes over the whole screen.

> **Note:** I have no background in C++ or Windows app development. This entire project was built through vibe coding with AI assistance. It's an early, experimental version — expect rough edges, and please don't expect production-quality code underneath.

## Features

- **Docked, always-on-top panel** — reserves screen space using the Windows AppBar API (`SHAppBarMessage`), the same mechanism the taskbar uses, so other windows resize around it instead of overlapping it.
- **Coexists with other docked bars** — if another AppBar (like the taskbar) docks to an edge, Deskplorer only repositions itself when its available working area actually *grows*; it never gets pushed by space it doesn't need to reserve.
- **Real Explorer view, embedded** — hosts an actual `IShellView` (the same view Explorer itself uses) rather than reimplementing a file browser, so file icons, thumbnails, drag & drop, and right-click actions behave exactly like native Explorer.
- **Tabs** — multiple folders open side by side in a compact, portrait-oriented tab bar, each with its own navigation history.
- **Back navigation** — a `Back` entry is injected at the top of the folder background's right-click menu, tied to a per-tab navigation history (not just "go to parent folder").
- **Dark context menus** — both Explorer's native context menu and the panel's own menus render in dark mode via `uxtheme.dll`.
- **Acrylic blur background** — a unified, translucent acrylic background across the whole panel.
- **Mouse wheel & touchpad scrolling** — manual scroll handling since the panel hides native scrollbars.
- **Quick folder/drive navigation menu** — a right-click menu for jumping straight into drives and common folders.

## How it works

Deskplorer is a small Win32 application written in C++ that uses the Windows Shell COM interfaces (`IShellFolder`, `IShellView`, `IContextMenu`, etc.) to embed real Explorer folder views inside its own window, rather than drawing its own file browser from scratch. Docking and space reservation is done through the AppBar API, the same one the taskbar and other system bars use.

## Build

Requires the Windows SDK (MSVC). Build from a Developer Command Prompt:

```
cl /EHsc /O2 /W3 /DUNICODE /std:c++17 Deskplorer.cpp /link user32.lib shell32.lib gdi32.lib ole32.lib oleaut32.lib dwmapi.lib comctl32.lib shlwapi.lib /SUBSYSTEM:WINDOWS
```

## Status

This is a personal project, built to scratch a specific itch. It works well for my own daily use but hasn't been tested widely across different Windows versions or multi-monitor setups. Issues and pull requests are welcome.

⚠️ **Note:** I have no background in C++ or Windows application development. This entire project was built through vibe coding with AI assistance, and it's an early, experimental version — expect rough edges, and please don't expect production-quality code underneath.

## License

This project is released into the public domain under [The Unlicense](https://unlicense.org/) — do whatever you want with it, no attribution required.
