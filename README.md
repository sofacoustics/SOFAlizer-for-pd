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
