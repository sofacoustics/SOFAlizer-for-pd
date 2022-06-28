# SOFAlizer-for-pd
 ## Introduction: SOFAlizer external for pure data

SOFAlizer~ is based on earplug~. https://puredata.info/downloads/earplug/releases/0.2
It is able to read in and process SOFA files. 

To compile SOFAlizer~ you need to download and build the library libmysofa.
https://github.com/hoene/libmysofa
Copy libmysofa.a into the directory SOFAlizer~ and run Makefile. 
Copy folder SOFAlizer~ into the directory for your pd externals.

bela_SOFAlizer is a project based on BelaOnUrHead. https://github.com/theleadingzero/belaonurhead
It runs on a Beagle Bone Black Board with Bela.
A sensor tracks head rotation and SOFAlizer processes HRTFs from a SOFA file according to the heads position.

Compile libmysofa and SOFAlizer on the Beagle Bone Black Board.
Copy pd_linux file into the directory ./projetcs/pd-externals.
Run _main.pd from directory ./projects/bela_SOFAlizer.

## Bela Board
Below you find instructions how to setup SOFAlizer-for-pd on a Bela Board.

### Connection to PC
Connect Bela board with
* LAN if you want to access via IDE, 
* via USB to a USB charger, or
* via USB to your PC

### Access
#### IP Addresses
If you don’t know them connect Board with USB to computer and use IDE in browser (http://192.168.6.2/) to find out IP addresses of device:

`ip address`

shows IP adresses of connection typs, example: see screenshot.

screenshot1

IPv4 addresses (`inet`) from the screenshot (IPv6 could also be used instead of IPv4):
3: usb0: 192.168.6.2		(for connection via USB)
2: eth0: 193.171.195.101 	(for connection via LAN)
#### IDE
Access to **IDE (Web-Interface)**: enter IP address: http://192.168.6.2/ (or LAN IP address, see above) in browser; files can be copied to Bela Board by drag-and-drop
#### SFTP
access via **SFTP**: same IP address (see above); user: root, no password
possible tools: Filezilla, putty, terminal,….

screenshot2

If access via SFTP is not working: check */etc/ssh/sshd_config*, it must contain (uncommented):

`PermitEmptyPasswords yes`

`UsePAM no`

### Update Bela Board
#### Download Bela board update (*master.zip*):
https://github.com/BelaPlatform/Bela/wiki/Updating-Bela 
extract *master.zip*
#### Update Bela Board:
Web-Interface: IP addresse, open in browser: http://192.168.6.2/ 
in browser window: "arrow" on right side → show menu → settings → Upload update Bela patch → select zip → install (takes a few minutes); 
(alternative installation possibility: run sh update_board → y)

* after updating Bela Board update system files:

    `sudo apt-get update`
    
    `sudo apt-get upgrade -y`

* install gcovr (obsolete?):

    `sudo apt-get install -y gcovr`
### Install Projects
General remark: new projects are created in: */root/Bela/projects* (file access via SFTP possible)
#### pd-externals
* Connect Bela Board with LAN and USB (to PC) 
* Open http://192.168.6.2/ in IDE (Browser) → File explorer → new project → “pd-externals” (any type)
* delete all files from *~/Bela/projects/pd-externals*
#### libmysofa
* download zip file: https://github.com/hoene/libmysofa  
* extract zip file
* [remark miho: I used the version from *P:\Projects\HRTF_measurement\SOFA\Software\SOFAlizer-for-pd\libmysofa-master 20200930*]
* Connect Bela Board with LAN and USB (to PC) 
* Open http://192.168.6.2/ in IDE (Browser) → File explorer → new project → “libmysofa” (any type)
* delete all files from */root/Bela/projects/libmysofa* (SFTP)
* copy extracted content from *libmysofa.zip* (via SFTP) to: */root/Bela/projects/libmysofa*
* in IDE (Browser): navigate to *~/Bela/projects/libmysofa#*
* in IDE enter (some commands may take some time, even a couple of minutes!):
    * `sudo apt install zlib1g-dev libcunit1-dev libcunit1-dev`
    * `cd build`
    * `cmake -DCMAKE_BUILD_TYPE=Release ..`
    * `make all test`
    * → Libraries are created; \*.a files are stored in */root/Bela/projects/libmysofa/build/src* 
(Some tests take long; file *libmysofa.a* will be needed for SOFAlizer later!)
### Install SOFAlizer
* download zip file: https://github.com/sofacoustics/SOFAlizer-for-pd
* extract zip file
* [remark miho: I used the version from *P:\Projects\HRTF_measurement\SOFA\Software\SOFAlizer-for-pd\SOFAlizer-for-pd-master 20201012*]
* Connect Bela Board with LAN and USB (to PC) 
* Open http://192.168.6.2/ in IDE (Browser) → File explorer → new project → “SOFAlizer_compile” (any type)
* delete all files from */root/Bela/projects/SOFAlizer_compile/* (SFTP)
* copy content from *\bela_SOFAlizer* subfolder (from extracted zip file) to this directory: */root/Bela/projects/SOFAlizer/*
* copy content from *\SOFAlizer~* subfolder (from extracted zip file) to this directory: */root/Bela/projects/SOFAlizer_compile/*
* Delete *libmysofa.a* from folder */SOFAlizer_compile/*
* Copy *libmysofa.a* from */root/Bela/projects/libmysofa/build/src* (see chapter libmysofa) into the directory */root/Bela/projects/SOFAlizer_compile/* (overwrite if existing)
* in directory */root/Bela/projects/SOFAlizer_compile/* edit address of LDFLAGS, and add library lgcov: in file Makefile (rows 34-35 in my case): → 
    `LDFLAGS += -L$(DIR)~/Bela/projects/SOFAlizer_compile -Wl,-R$(DIR)~/Bela/projects/SOFAlizer_compile '-Wl,-R$$ORIGIN'`
    `LIBS = libmysofa.a -lz -lgcov`
* run Makefile (command: `make`) in folder of Makefile file
* SOFAlizer~.pd_linux wird erstellt, kopieren nach *~/Bela/projects/pd-externals*:
    `cp SOFAlizer~.pd_linux ~/Bela/projects/pd-externals`
    
### Update existing SOFAlizer
* Download: https://github.com/sofacoustics/SOFAlizer-for-pd download zip file
* extract
* http://192.168.6.2/ (IDE) im Browser öffnen → File explorer
* navigate to: */root/Bela/projects/SOFAlizer_compile/*
* drag and drop file (for example *SOFAlizer~.c*) to the project window in browser and overwrite
* optional (if not done during installation, details see previous chapter): 
    * Copy *libmysofa.a*
    * edit address of LDFLAGS in *makefile*
* run *Makefile* (command: `make`) in folder of *Makefile* file
* *SOFAlizer~.pd_linux* wird erstellt, kopieren nach *~/Bela/projects/pd-externals*: 
`cp SOFAlizer~.pd_linux ~/Bela/projects/pd-externals`

### Change blocksize of pd
Needs to be done just once, for every Bela board, should not change.
* navigate to folder */root/Bela/*
* enter the following commands:
    * `cd ~/Bela`
    * `git clone https://github.com/BelaPlatform/libpd.git`
    * `cd libpd`
    * `git submodule init` (takes some time!)
    * `git submodule update --recursive` (takes some time!)
* Filezilla:
    * open *~/Bela/libpd/pure-data/src/s_stuff.h* → edit
    * change to desired internal block size: 
    `#define DEFDACBLKSIZE 16`
    change to:
    `#define DEFDACBLKSIZE 32`
* IDE: in folder *~/Bela/libpd* enter:
    * `make -f Makefile-Bela` (Takes a couple of minutes! Last command is longer with many parameters)
    * `make -f Makefile-Bela install` (done after if loop)
* IDE: right in Project Settings menu (gearwheel symbol): change Block size (audio frames) to same value (32 for instance), or higher

### Run SOFAlizer
#### Prepare hardware
* mount tracker on top of headphones
* connect headphones to out connection
* connect audio input to an audio device
#### With a computer
* IDE: Navigate to project SOFAlizer
* select *_main.pd*
* run (refresh button left at the bottom)
* takes ~3 minutes to run!!
#### Without a computer
To run SOFAlizer without a computer, configure first in IDE:
* right in Project Settings menu (gearwheel symbol)
* scroll down to “Run project on boot”
* set: *SOFAlizer*
#### Bela Board
* when switching on the Bela Board give it a few minutes to load the SOFA file
* button ‘trigangle’ on Bela Board: switch between standby and spatialize
* button ‘dot’ on Bela Board: reset position to 0° / 0°
