# SOFAlizer-for-pd
 ## Introduction: SOFAlizer external for Pure Data
 
This project presents an interactive, low-cost, binaural, virtual acoustics embedded system through headphones. It utilizes an embedded computer with low-latency audio  and a low-latency head-tracker. For binaural real-time synthesis a discrete circular convolution algorithm is applied. It caculates the sequence of a head-related transfer function (HRTF), with a backwards shifted sequence of a ring-buffer of the same size, that stores the audio input signal, with a multiply–accumulate (MAC) function. Because of the short filter lengths convolution in the time domain is suffcient. The code is written in C for Pure Data and is able to handle SOFA (Spatially Oriented Format for Acoustics) files. This work can serve as a tutorial on how to program a Pure Data external. The system resources are sufficient enough to handle SOFA databases up to 1550 positions and a filter length of 128 taps for three virtual loudspeakers. A filter length of 128 taps is considered as accurate representation of HRTFs. With this configuration the overall latency of the system is 41 ms, which is low enough to be adequate for most virtual audio applications. Interpolation of HRTFs between positions has not been considered in this project. To process larger SOFA databases with more positions for example with offline interpolated HRTFs, either the number of virtual loudspeakers has to be reduced or the processed block-size has to be increased, whereby the latter extends the latency of the system.

### Credits: 
* SOFAlizer~ is based on the Pure Data external earplug~. https://puredata.info/downloads/earplug/releases/0.2
* Sofalizer~ utilizes the library libmysofa to handle SOFA files. https://github.com/hoene/libmysofa
* bela_SOFAlizer is based on the project BelaOnUrHead. https://github.com/theleadingzero/belaonurhead

## Hardware
Bela - A Beagle Bone Black with Xenomai real-time kernel extensions and with custom expansion board  
&emsp;&emsp;&emsp;for synchronous ultra low-latency audio and sensor processing at audio rate. As audio and sensor data  
&emsp;&emsp;&emsp;is processed simultaneously, no jitter is introduced between them.  
&emsp;&emsp;&emsp;Further it features two "Programmable Real-time Units " (PRUs), that are 200 MHz microcontrollers  
&emsp;&emsp;&emsp;on the same chip as the CPU, with access to the same memory and peripherals as the CPU.     
&emsp;&emsp;&emsp;A custom audio codec and sensor ADC/DAC driver utilizes the BeagleBone PRU and acts as a DMA  
&emsp;&emsp;&emsp;(Direct Memory Access) controller, that transfers data between the hardware and a memory buffer.       
&emsp;&emsp;&emsp;A Xenomai real-time audio and sensor task can process data from this buffer and is able to run at  
&emsp;&emsp;&emsp;higher priority than the Linux kernel.  
&emsp;&emsp;&emsp;So Pure Data's audio callback function "DSP_perform" runs as Xenomai function at the highest priority.    
&emsp;&emsp;&emsp;**https://bela.io/**    

Adafruit 9-DOF Absolute Orientation IMU Head-tracker sensor: **https://www.adafruit.com/product/2472**  

### Part List

QTY  | PARTS
---- | -------------
  1  | Bela Starter Kit
  1  | Adafruit 9-DOF Absolute Orientation IMU Fusion Breakout – BNO055 [ADA2472]
  1  | Breadboard
  2  | Push Buttons
  2  | Resistors 1.5k
  4  | Wire 1.5 m (SPI)
  4  | Wire 15 cm
  2  | Wire 5 cm
  1  | Headphones

<br/>

## Hardware Assembly

<br/>

![image](https://user-images.githubusercontent.com/58428856/195568403-2da09cb6-724a-4ddf-8353-60048f0df749.png)

<br/>

## Setup SOFAlizer-for-pd on Bela

### Network Connections
Connect Bela board via
* Ethernet to a network device that provides access to the internet.
* USB to a PC to power it on and to provide access to the onboard IDE through a webbrowser.

### Access
#### IP Addresses
Determine the IP addresses assigned to the availiable network interfaces:
* Connect to the onboard Board IDE: Open a webbrowser and go to http://bela.local/ or http://192.168.6.2/.
* In the command prompt at the bottom of the site type:
*  `ip address`

The output shows the IP adresses of all availiable network interfaces: see screenshot.

![bela1](https://user-images.githubusercontent.com/12407858/176189162-0264fbe9-0651-4cc3-9fd2-b048684279ec.jpg)

IPv4 addresses (`inet`) from the screenshot are: <br/>
* 3: usb0: 192.168.6.2		(for connection via USB)
* 2: eth0: 193.171.195.101 	(for connection via Ethernet)

(IPv6 addresses will work as well.)
#### IDE
To access Bela's **IDE Web-Interface** open a webbrowser and go to http://bela.local/ or http://192.168.6.2/ 
<br/>
Files can be copied to the Bela Board by simply drag-and-drop.
#### SFTP
Access Bela via **SFTP**:
* IP address: 192.168.6.2
* Port: 22
* user: root
* no password 

Possible SFTP applications are: Filezilla, putty, terminal,….
<br/>

![bela2](https://user-images.githubusercontent.com/12407858/176189245-2828d993-845b-4cc7-8b0b-4100cafd6170.jpg)

If access via SFTP is not working, check */etc/ssh/sshd_config*. It must contain (uncommented):

`PermitEmptyPasswords yes`

`UsePAM no`

### Updating Bela
#### Download the image file with the latest version of the Bela core code and the IDE to a PC from:
* https://github.com/BelaPlatform/Bela
* Click **Clone or download** and then **Download ZIP**

#### Update the Bela Board:
* Connect the Bela Board via Ethernet to the internet and via USB to the PC;
* Connect to Bela's **IDE Web-Interface**: Open a webbrowser and go to http://bela.local/ or http://192.168.6.2/.
* Click on the **gearwheel icon** on the top right bar to open the **settings contextmenu**.
* Scroll down the settings menu and click on the option **Upload an update patch**: Select the image file *Bela-master.zip* from the PC and click **Upload**
* Once the upload is complete, you will need to refresh the browser. **Note: This may take several minutes!** <br/>

**An alternative installation method** is to copy the image file *Bela-master.zip* from the PC to the directory *~/Bela/updates* on the Bela board: <br/>
On a Linux PC open a terminal window and change to the directory, where the image file is stored: <br/>
* `cd <dir>`
 
Copy the image file *Bela-master.zip from the PC to the dedicated directory on the Bela board with the secure copy command: <br/>
* `sudo scp ./Bela-master.zip root@192.168.6.2:~/Bela/updates`

Alternatively use your favorite SFTP application with configuration as described above. <br/>

When the file upload is complete, connect from PC either to Bela's IDE via a webbrowser or use the ssh-client in the terminal window:
* `ssh root@192.168.6.2`
*  confirm fingerprint with `yes`

After gaining access to the Bela board change to the diretory, where the update script is located and run it:
* `cd ~/Bela/scripts`
* `sh update_board`
* confirm with `y`

After updating the Bela Board, update its system files:
* `sudo apt-get update`  
* `sudo apt-get upgrade -y`

Install gcovr:
* `sudo apt-get install -y gcovr`
 
### Create New Projects on the Bela board
General remark: New projects are created in directory */root/Bela/projects* on the Bela board. (File access via SFTP is possible)
#### Create location for pd-externals
* Connect the Bela Board via Ethernet to the internet and via USB to the PC.
* Connect to Bela's **IDE Web-Interface**: Open a webbrowser and go to http://bela.local/ or http://192.168.6.2.<br/>
* Click on the **file icon** on the top right bar to open the **Project explorer** contextmenu → click on **New project** → Enter project name: *pd-externals* (select any type).
* Delete all files from the directory *~/Bela/projects/pd-externals*. (From IDE or via SFTP)
#### Install SOFA library libmysofa
* Download the zip file from https://github.com/hoene/libmysofa to the PC.
* Click **Clone or download** and then **Download ZIP**.
* Extract the zip file to a convenient directory on the PC.
 
[remark miho: I used the version from *P:\Projects\HRTF_measurement\SOFA\Software\SOFAlizer-for-pd\libmysofa-master 20200930*]
 
* Connect the Bela Board via Ethernet to the internet and via USB to the PC.
* Connect to Bela's **IDE Web-Interface**: Open a webbrowser and go to http://bela.local/ or http://192.168.6.2.
* Click on the **file icon** on the top right bar to open the **Project explorer** contextmenu → click on **New project** → Enter project name: *libmysofa* (select any type).
* Delete all files from the directory */root/Bela/projects/libmysofa*. (From IDE or via SFTP)
* Copy the extracted content from *libmysofa.zip* on the PC to the directory */root/Bela/projects/libmysofa* on the Bela board via SFTP.
* Connect to the IDE and navigate in the command prompt to the directory *~/Bela/projects/libmysofa*. <br/>

Then enter the following commands:
* `sudo apt install zlib1g-dev libcunit1-dev libcunit1-dev`
* `cd build`
* `cmake -DCMAKE_BUILD_TYPE=Release ..`
* `make all test`

**Note: This may take several minutes!** <br/>
Don't worry, if some tests fail. <br/>
libmysofa libraries are created in */root/Bela/projects/libmysofa/build/src*. <br/>
Static library *libmysofa.a* will be used for SOFAlizer later on! <br/>
 
### Install SOFAlizer~
* Download the zip file from https://github.com/sofacoustics/SOFAlizer-for-pd to the PC.
* Click **Clone or download** and then **Download ZIP**.
* Extract the zip file to a convenient directory called SOFAlizer-for-pd on the PC.

 [remark miho: I used the version from *P:\Projects\HRTF_measurement\SOFA\Software\SOFAlizer-for-pd\SOFAlizer-for-pd-master 20201012*] <br/>
 
#### Create two new Projects on the Bela board:
**First Project:** <br/>

* Connect the Bela Board via Ethernet to the internet and via USB to the PC.
* Connect to Bela's **IDE Web-Interface**: Open a webbrowser and go to http://bela.local/ or http://192.168.6.2.
* Click on the **file icon** on the top right bar to open the **Project explorer** contextmenu → click on **New project** → Enter project name: *SOFAlizer_compile* (select any type).
* Delete all files from the directory */root/Bela/projects/SOFAlizer_compile/*. (From IDE or via SFTP)
* Copy/Upload the content from the subdirectory *./SOFAlizer~* of the directory *./SOFAlizer-for-pd* on the PC to the directory */root/Bela/project/SOFAlizer_compile/* on the Bela board. (SFTP or IDE)
* Delete the library *libmysofa.a* from the directory */SOFAlizer_compile/*

Copy the library *libmysofa.a* from the directory */root/Bela/projects/libmysofa/build/src* (generated in chapter libmysofa) into the directory */root/Bela/projects/SOFAlizer_compile/* from command prompt: (overwrite if existing)
* `cp /root/Bela/projects/libmysofa/build/src/libmysofa.a /root/Bela/projects/SOFAlizer_compile/`

Go to IDE and select in **Project explorer** (**file icon** on the top right bar) the Project *SOFAlizer_compile*. <br/>
Select *Makefile* and edit in IDE the *addresses* of the *LDFLAGS* and add at *LIBS* the *library lgcov* <br/>

Makefile (rows 34-35):
* `LDFLAGS += -L$(DIR)~/Bela/projects/SOFAlizer_compile -Wl,-R$(DIR)~/Bela/projects/SOFAlizer_compile '-Wl,-R$$ORIGIN'`
* `LIBS = libmysofa.a -lz -lgcov`

Run Makefile from command prompt:
* `cd ~/Bela/projects/SOFAlizer_compile`
* `make`

The PD external *SOFAlizer~.pd_linux* is created. Copy *SOFAlizer~.pd_linux* to *~/Bela/projects/pd-externals*:
* `cp ~/Bela/projects/SOFAlizer_compile/SOFAlizer~.pd_linux ~/Bela/projects/pd-externals`

**Second Project:** <br/>

* Click on the **file icon** on the top right bar to open the **Project explorer** contextmenu → click on **New project** → Enter project name: *SOFAlizer* (select any type).
* Delete all files from the directory */root/Bela/projects/SOFAlizer/*. (From IDE or via SFTP)
* Copy/Upload the content from the subdirectory *./bela_SOFAlizer* of the directory *./SOFAlizer-for-pd* on the PC to the directory */root/Bela/projects/SOFAlizer/* on the Bela board. (SFTP or IDE)

### Change the internal block size of Pure Data
**Note:** An internal block size of 16 samples is not sufficient for this task. <br/>
The internal block size of Pure Date should be set to 64 samples, because this is the minimum block size Pure Data is processing data. <br/>
Even if the internal block size is set to 32 samples or to 16 samples, the actual delay of the filtering process remains the same regardless of the configured audio buffer size. <br/>
Setting a smaller internal block size than 64 samples in Bela means, that the libpd callback will effectively process data every four (with 16 samples per block) or two (with 32 samples per block) Bela audio callbacks. <br/>
Even though the minimum internal block size corresponds to the minimum audio buffer size, the lower latency achieved with an audio buffer size of 32 samples compared to a audio buffer size of 64 samples will make a difference of 1.437 ms. <br/>
 
* Connect the Bela Board via Ethernet to the internet and via USB to the PC.
* Connect to Bela's **IDE Web-Interface**: Open a webbrowser and go to http://bela.local/ or http://192.168.6.2.

Navigate to folder */root/Bela in command prompt:
* `cd ~/Bela`
* `git clone https://github.com/BelaPlatform/libpd.git`
* `cd libpd`
* `git submodule init` (takes some time!)
* `git submodule update --recursive` (takes some time!)

Connect from PC via terminal to Bela board:
* `ssh root@192.168.6.2`

Open *~/Bela/libpd/pure-data/src/s_stuff.h*
* `nano ~/Bela/libpd/pure-data/src/s_stuff.h`

Scroll down to line #define DEFDACBLKSIZE and change block size from: 
* `#define DEFDACBLKSIZE 16` to
* `#define DEFDACBLKSIZE 64`
* press `ctrl + x` to exit
* type `y` to confirm changes
* press `enter` to `save s_stuff.h`
* type `exit` to `close ssh connection`


* Connect to Bela's **IDE Web-Interface**: Open a webbrowser and go to http://bela.local/ or http://192.168.6.2.

Navigate to folder *~/Bela/libpd* from command prompt:
* `cd ~/Bela/libpd` 

Type:
* `make -f Makefile-Bela` (Takes a couple of minutes! Last executed command from *MAKEFILE* takes longer.)
* `make -f Makefile-Bela install` (done after `if loop`)

* In IDE click on the top right bar the **gearwheel icon**: Change *Block size (audio frames)* to the same value as PD's internal default block size: 64

### Run SOFAlizer
#### Prepare hardware
* Mount tracker on top of the headphones
* Connect headphones to the audio output of the Bela board 
* Connect an audio playback device to the audio input of the Bela board
#### From a PC
* Connect to Bela's **IDE Web-Interface**: Open a webbrowser and go to http://bela.local/ or http://192.168.6.2.
* Click on the top right bar the **file icon**: Open existing project: *SOFAlizer*
* Select the file *_main.pd*
* Press `Run` (It's the button with the **refresh icon** left at the bottom, just above the command prompt)
* It takes ~3 minutes to read in two SOFA files until the audio processing is starting
#### Without a PC
* Connect to Bela's **IDE Web-Interface**: Open a webbrowser and go to http://bela.local/ or http://192.168.6.2.
* Click on the top right bar the **gearwheel icon**
* Scroll down to option *Run project on boot*
* Select: *SOFAlizer*

Type in command prompt: 
* `shutdown --reboot now`
* When Bela Board reboots give it a few minutes to load the SOFA files

#### Project SOFAlizer is running
* When the project is running and the SOFA files have successfully been loaded, you should hear the audio signal, provided from your audio play-back device on your headphones 
* Push the calibration button in the middle to reset the head-tracker position to 0°/0° twice or until the sound source is positioned directly infront of you.
* Start moving around your head and notice that the sound source stays in place and is perceived in the spatially correct position, according to your head position.
* To bypass spatialzation by SOFAlizer~ push the bypass button on top 
* To switch between different HRTF sets defined in _main.pd push the switch button on the bottom

### Update existing SOFAlizer
* Download the zip file from https://github.com/sofacoustics/SOFAlizer-for-pd to the PC.
* Click **Clone or download** and then **Download ZIP**.
* Extract the zip file to a convenient directory called SOFAlizer-for-pd on the PC.
* Connect the Bela Board via Ethernet to the internet and via USB to the PC.
* Connect to Bela's **IDE Web-Interface**: Open a webbrowser and go to http://bela.local/ or http://192.168.6.2.
* Click on the top right bar the **file icon** and open existing project: *SOFAlizer_compile*
* Drag and drop or press *Upload file" *SOFAlizer~.c* to the project window in browser and confirm to overwrite
* Upload any file that has changed in SOFAlizer-for-pd  to its corresponding directory

If libmysofa.a has changed follow the steps in ***Install SOFA library libmysofa***  and copy *libmysofa.a* to */root/Bela/projects/SOFAlizer_compile/*
* `cp /root/Bela/projects/libmysofa/build/src/libmysofa.a /root/Bela/projects/SOFAlizer_compile/`
*  Edit addresses of LDFLAGS in *Makefile* if necessary

Run *Makefile from command prompt*:
* `cd ~/Bela/projects/SOFAlizer_compile/`
* `make`

The PD external *SOFAlizer~.pd_linux* is created. Copy *SOFAlizer~.pd_linux* to *~/Bela/projects/pd-externals*: 
* `cp ~/Bela/projects/SOFAlizer_compile/SOFAlizer~.pd_linux ~/Bela/projects/pd-externals`

Everything is set up now and you can `run` the *_main.pd* file from project SOFAlizer
* In IDE click on the top right bar the **file icon** and open existing project: *SOFAlizer*
* Select the *_main.pd* file
* Press button with **refresh icon** left on the bottom just above the command prompt to `run` the system
* Enjoy!
