# NetCoin for Windows

## Ready-made binaries

Download `netcoin-<version>-win64.zip` from the
[Releases](https://github.com/pwarszawski/netcoin/releases) page, unpack it
anywhere and start `netcoin.exe` (graphical wallet) or `netcoind.exe`
(command-line node). Keep the `.dll` files and the `platforms` folder next
to the programs. Check the download against `SHA256SUMS.txt`
(PowerShell: `Get-FileHash netcoin-<version>-win64.zip`).

The data directory is `%APPDATA%\NetCoin`, the same as the old 3.0.0
client. An old wallet is converted to the new format on the first start
(see [build-ubuntu-24.04.md](build-ubuntu-24.04.md#wallet-format-sqlite)).

## How the binaries are made

The GitHub workflow `.github/workflows/release.yml` builds them with
[MSYS2](https://www.msys2.org/) (UCRT64 toolchain and libraries). It runs
only when the repository owner starts it (Actions → Release → Run
workflow) or pushes a tag `v*`. Every push also builds and tests the
Windows version (workflow "Build"); those test builds are kept for 7 days
as workflow artifacts.

## Building it yourself

1. Install MSYS2 and open the **MSYS2 UCRT64** shell.
2. `pacman -S git`
3. `git clone -b ubuntu-24.04-revival https://github.com/pwarszawski/netcoin.git`
4. `cd netcoin && bash contrib/ci/build-windows.sh mybuild`

The script installs the required packages, builds `netcoind.exe`, runs the
unit tests, builds `netcoin.exe` and puts everything needed into
`dist/netcoin-mybuild-win64/` and a `.zip`.
