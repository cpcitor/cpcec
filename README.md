# CPCEC

An emulator by CNGSOFT under GPLv3 license.

This repository was created by a script turning the zip files available on [http://cngsoft.no-ip.org/cpcec.htm](http://cngsoft.no-ip.org/cpcec.htm) into a git history.

The emulator can compile on Windows and SDL2-supported platforms including Linux desktop.

See [cpcec.txt](cpcec.txt) for instructions in English, including build instructions.

Below is a snapshot of [http://cngsoft.no-ip.org/cpcec.htm](http://cngsoft.no-ip.org/cpcec.htm) turned into markdown format.

------------------------------------------------------------------------

---
---

![](http://cngsoft.no-ip.org/cpcec192.png) ![](http://cngsoft.no-ip.org/zxsec192.png) ![](http://cngsoft.no-ip.org/csfec192.png) ![](http://cngsoft.no-ip.org/msxec192.png)  
**CPCEC** Amstrad CPC emulator  
and its siblings **ZXSEC**, **CSFEC** and **MSXEC**  
  
  

## Foreword

**CPCEC** is an emulator of the family of home microcomputers **Amstrad CPC** (models 464, 664, 6128 and Plus) whose goal is to be loyal to the original hardware and efficient in standard modern systems. Thus it brings a faithful emulation of the Z80 microprocessor and it replicates the behavior of the CRTC 6845 and Gate Array video chips, the PSG AY-3-8912 sound chip, the remaining circuits found in the original hardware, and the tape deck and floppy disc drive that made possible loading and running software.

CPCEC includes several related projects. **ZXSEC** is an emulator of the **Sinclair Spectrum family (48K, 128K, +2/Plus2 and +3/Plus3)** based on the components it shared with the Amstrad CPC family: the Z80 microprocessor, the PSG AY-3-8912 sound chip, the tape system and the NEC765 disc drive controller; **CSFEC** is an emulator of the **Commodore 64** platform, similarly based on shared code; and **MSXEC** is an emulator of the **MSX family (1983 MSX, 1985 MSX2, 1988 MSX2+)**, also based on shared code.

The default build of CPCEC requires a Microsoft Windows 2000 operating system or later, while the SDL2 build requires any operating system supported by the SDL2 library. The minimal hardware requirements are those fitting the operating system, and it's advised that the main microprocessor runs at 400 MHz at least. Screen resolution in pixels must be 800x600 at least. A sound card is optional. Using a joystick is optional, too.

Software and documentation are provided "as is" with no warranty. The source code of CPCEC and its binaries follow the **GNU General Public License** v3, described in the file GPL.TXT within the package.

## Gallery

See [original CPCEC page](http://cngsoft.no-ip.org/cpcec.htm) for an image gallery.

## Acknowledgements

This emulator owes its existence to a series of people and societies that are listed as follows:

- The firmware files included in the package are **Amstrad**'s properties, who allows the emulation of their old computer systems and supports the distribution of their firmwares as long as their authorship and contents are respected, and whom I wholeheartedly thank the creation of those magnificent computers and the good will towards their emulation.
- This emulator was my final project for the Computer Engineering postdegree at the **Distance-Learning National University (Universidad Nacional de Enseñanza a Distancia, UNED)**, a project directed by professor **José Manuel Díaz Martínez** and ultimately awarded a 100% and the right to a honorable mention.
- The documentation about the system comes from **cpcwiki.eu, cpc-power.com, cpcrulez.fr** and **quasar.cpcscene.net**.
- The alpha tests were handled by **Denis Lechevalier**.

## Version log

See [git history](https://github.com/cpcitor/cpcec/commits)

------------------------------------------------------------------------

\[ [amstrad.es](http://www.amstrad.es/forum/viewtopic.php?t=5332) \| [cpcrulez.fr](http://cpcrulez.fr/forum/viewtopic.php?t=6195) \| [cpcwiki.eu](http://www.cpcwiki.eu/forum/emulators/cpcec-a-new-emulator-from-cngsoft/new#new) \| [CPCEC Git archive (cpcitor)](https://github.com/cpcitor/cpcec) \| Norecess464: [CPCEC-GTK](https://gitlab.com/norecess464/cpcec-gtk) + [CPCEC-PLUS](https://gitlab.com/norecess464/cpcec-plus) \]

------------------------------------------------------------------------

  
[Spanish CPC firmware](http://cngsoft.no-ip.org/cpc-rom-esp.zip) [French CPC firmware](http://cngsoft.no-ip.org/cpc-rom-fra.zip)

------------------------------------------------------------------------

[Send a comment to the original author of cpcec](/comments.htm)
