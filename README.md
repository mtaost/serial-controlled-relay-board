# Serial Controlled Relay Board
![](https://raw.githubusercontent.com/mtaost/serial-controlled-relay-board/master/images/assembled_board.jpg)

This device is a command line controlled device capable of controlling 8 low current reed relays. This was a commissioned project which was meant to help with testing of computer systems which frequently needed their boot configurations switched through hardware jumpers. The normal procedure involved shutting down the system remotely, then walking over to the lab to remove the system from the chassis and manually setting jumpers which looked something like this: 

![alt](https://qph.fs.quoracdn.net/main-qimg-f26eee1a1ad8fb1df27e0bfab09a742e.webp)

Doing this not only was time consuming, but also introduced the system to risk of damage from physical wear and tear, and ESD dangers. 
With the SCRB, one can remotely control the state of whatever jumpers are on the board through a command line interface. For example: 

    $ Serial Relay Controller Ready
    
    $ status
	    J1:		OFF
	    J2:		OFF
	    J3:		OFF
	    J4:		OFF
	    J5:		OFF
	    J6:		OFF
	    J7:		OFF
	    J8:		OFF
	    
	$ set J1 on
	J1: OFF -> ON
After this series of commands, the jumper in position J1 will now be connected, simulating a connected jumper. The board also supports renaming jumpers: 

    $ rename J1 write_protect
    J1 renamed to: write_protect
    $ status
    	write_protect:		ON
	    J2:		OFF
	    J3:		OFF
	    J4:		OFF
	    J5:		OFF
	    J6:		OFF
	    J7:		OFF
	    J8:		OFF

This could also be used for other, low current/voltage switching applications. 
## Circuit design
The SCRB uses a simple ATmega 328PU MCU as its brains. This is the same microprocessor used on the Arduino Uno which I happened to have a lot of which made it easy to prototype and test. Communication with the serial connection is managed through a MAX232 level converter which is necessary to lower the higher voltage levels of the RS232 standard (-13 to +13 volts) to a more TTL friendly 0-5v. To handle the switching of the relays, a Darlington transistor array ensures that the microprocessor is not damaged by the inductive kickback of the relays (however small it might be). 
![Schematic of processing side of SCRB](https://raw.githubusercontent.com/mtaost/serial-controlled-relay-board/master/images/sch_closeup.png)

The relays used are 500mAh, 200VDC reed relays. In their application as jumper connectors, they really only need to switch 3.3VDC and negligible current but these were the most suitable relays I could find. An array of LEDs lets you know when each relay is working. 

![Schematic of relay side of SCRB](https://raw.githubusercontent.com/mtaost/serial-controlled-relay-board/master/images/sch_closeup_2.png)

