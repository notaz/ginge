
GINGE - Ginge Is Not GP2X Emulator
release 4

(C) notaz, 2010-2011
http://notaz.gp2x.de/


About
-----

Ginge is an application that can run many GP2X F100/F200, Wiz games and
programs on other ARM Linux platforms, which currently includes Pandora
Caanoo and Wiz itself. It is not a full hardware emulator like MAME, PicoDrive
or similar, it does not emulate the CPU. It can be considered as compatibility
layer similar to Wine on PC Linux, however it does emulate small portion of
MMSP2 and Pollux system-on-chips. It operates by hooking certain system calls
and using realtime patching of code that accesses memory mapped hardware
directly.


Usage
-----

Ginge comes with a launcher that is started when you run Ginge. The launcher
can then be used to start GP2X software, which will either run if it's
compatible, or just return back to the menu if it is not. In some cases it
might hang though.

Keys are mapped to corresponding keys pandora, Wiz and Caanoo , except:

Key              Pandora   Wiz              Caanoo
Stick Push       1         unmapped         Stick Push
Volume up/down   '.', ','  Volume up/down   Home+I/Home+II

On pandora pressing 'q' will exit the menu or try to kill current application.
On Cannoo Home+Y tries to kill current application.


Structure
---------

Ginge actually consists of 4 independent executables and a few scripts:

+ ginge_sloader - loader of static executables
+ ginge_dyn     - dynamic executable handler
+ ginge_prep    - .gpe parser that selects the right handler from above
+ gp2xmenu      - the launcher/menu program
+ ginge_dyn.sh  - environment setup script for ginge_dyn
+ ginge.sh/gpe  - menu launcher script

The menu is optional and can be replaced or bypassed completely. The only thing
it does is running ginge_prep on GP2X .gpe program, ginge_prep handles the rest.


Changelog
---------

r4
+ ginge now runs on Caanoo
* minor fixes in path handling

r3
* improved exec handling, mostly for gpecomp.
+ added preliminary Wiz support, pcsx4all works.
* Wiz: since some stuff is written to /tmp, mount tmpfs there when starting
  to avoid wearing down flash.

r2
* improved exit handling
* Wiz: should now return to Wiz menu after gp2xmenu exit

r1 - initial release
+ icons provided by Inder


License
-------

gp2xmenu is based on GPH GPL source (http://www.gnu.org/licenses/gpl.html).
Source is available at http://notaz.gp2x.de/releases/ginge/gp2xmenu.tar.bz2

Ginge may come with some libraries. Those libraries are unmodified copies
of ones found in root filesystems in GP2X and Wiz and are included to more
accurately reproduce environment found on GP2X. Their source code may or may
not be available, I did not I use it, but whatever I found is mirrored here:
http://notaz.gp2x.de/downloads/gp2x/src/410_all/

Remaining portion is released under custom closed source license. It is not
derived from gp2xmenu and is completely standalone, the menu is only included
for user's convenience.

Redistribution and use of program's binaries and helper scripts, with or without
modification, is permitted provided that the following conditions are met:
  * This readme is included in unmodified form.
  * The program in any of it's forms is not sold or used as part of any
    commercial package, including pre-installed or included in any kind of
    portable device.
  * It is not bundled and distributed with any GP2X or Wiz program without
    respective program's author's permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDER BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

