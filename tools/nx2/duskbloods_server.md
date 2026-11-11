# Local Duskbloods servers in the Qt build

The Qt executable starts local servers when `duskbloods_server.json` exists beside it.
The helper `duskbloods_server.py` is copied there by CMake. This machine's build also has
`duskbloods-local/`, a copy of the existing launcher's Python server files, and a local
configuration pointing at those files and the installed Python environment.

The JSON configuration has four absolute paths:

- `python`: Python interpreter with `grpcio` and `cryptography` installed.
- `launcher`: the existing `play_duskbloods.py`, with its `tools/` directory beside it.
- `state`: writable directory for certificates and per-run server data and logs.
- `pki`: existing test certificates to copy on first launch; if absent, generate new ones.

The helper starts only the servers, reports their connection settings to the emulator, and
waits for its stdin pipe to close. Closing or killing the emulator closes that pipe and
stops the helper's own servers. The emulator waits for server startup before constructing
its main window, so a game opened from the command line gets the same connection settings.
Normal emulator settings and saves are used.

An existing `EDEN_NX2_ONLINE_SERVER` environment value takes precedence and skips automatic
startup, allowing the original launcher and remote-server diagnostics to keep working.
Removing or renaming `duskbloods_server.json` disables automatic local servers for this build.
The Python environment and server files remain required; the executable is not a standalone
server distribution.
