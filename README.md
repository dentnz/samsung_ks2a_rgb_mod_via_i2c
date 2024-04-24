# RGB Modding Samsung CS-29D8W - KS2A Chassis - VDP313XY - I2C Hijacking

Karl Lurman - 2024

**Warning - do not open a CRT unless you know what you are doing. CRT televisions retain high-voltages in the tube even when disconnected from power. Exposure to these voltages will cause death.**

The Australian version of this consumer set comes wih double RCA AV composite inputs. These are soldered onto the chassis, using a plastic housing block with tabs to keep it sturdy. The solder location is configured in the typical SCART shape, implying other regions have SCART sockets in place of the RCAs.

The obvious conclusion is that it's entirely possible to replace the composite video inputs with RGB. Reconstructing the path for the required RGB signals *should* enable RGB video for this set.

## Hardware modifications

<< RGB hardware modifiction details to be inserted>>. Thanks to DJ Calle for supplying parts and advice on the required changes.

After performing the necessary hardware changes to reconstruct the missing RGB path, attempting to use RGB scart results in a composite video signal only. Blanking at various voltage values will **not** enable RGB.

It appears that the set is "software locked", ignoring the blanking input. 

## I2C hacking process

First, I located the datasheet for the Micronas VDP313XY, see VDP313XY_ETC.pdf. Reading through the documentation reveals the VDP communicates with other components on the chassis via the typical I2C messaging.

The next step was to attach a Bus Pirate to the I2C bus and to start to play with the VDP's internal registers to see what can be achieved. I added wires to VDP pins 3,4,5 for SCL/clock, SDA/data, GRND/ground respectively, and got hacking.

The VDP interface provides two slave addresses, 0x8A for the backend, and 0X8e for the FastProcessor. The datasheet describes the structure of each "I2C packet", but in summary:

Writing to 8bit Backend Register, using Bus Pirate syntax:

```
[ 0x8a 0x31 0b000001101 ]
```

N.B relevant pull-up resistor settings are excluded for simplicity.

This will set register 0x31, FBMOD2, fast blank interface mode 2. Bit 0 set to 1 forces internal fast blank 2 signal to high. Bit 2 set to 1 ensures FBLIN2 > FBLIN1. Bit 3 set to 1 forces monitoring of FBLIN2 pin for fast blanking.

If an RGB signal is fed into the set, and AV mode is active with a valid composite signal input, then the above packet will enable RGB on the reconstructed RGB path. AV1 will switch to RGB mode as expected! However, there are several issues:

- If RGB is enabled, MICOM OSD (e.g menu) is not visible due to a priority issue, with it being displayed under the RGB picture.
- The process of sending the message is not reliable due to the bus being "busy" with what looks like watchdog messages coming from the MICOM chip.
- We will need to replace the Bus Pirate with a cheaper, more permanent, solution.
- We must be in AV mode when RGB is enabled, composite sync does not appear to be receieved for the incoming RGB signal. Unfortunately, the set does not remember AV input selection upon shut-down, even if "AV accessible by channel select" is activated in the service menu.
- The set does not remember AV settings for the service menu, making service menu configuration with an incoming RGB signal impossible.

The first issue can be solved by setting bits on another backend register, 0x4b, OSDPRIO. By adding the following to our packet, we set the priority of the OSD to be in front of the picture frame:

```
[ 0x8a 0x31 0b000001101 0x4b 0b000000001]
```

Pressing the menu key on the remote will now display the OSD with correct priority, over the RGB picture frame.

The issue of the busy I2C bus required examination of the KS2A chassis and Micronas SDA5555 MICOM datasheets. This revealed that pin 4, also known as Bus-Stop, would be a likely mechanism for disabling the chips I2C bus access. The thought being, that with this chip disabled, my additional "master-like" messages injected onto the bus, would make it to the VDP uninterrupted. It should also be noted that if disabled, the MICOM cannot control the television in any way, including responding to Remote Control or Front Panel inputs.

The Bus Pirate had a handy output pin which I purposed for a digital signal to the active-low Bus Stop pin on the MICOM. Sure enough, setting the output signal to 0/LOW resulted in the bus going silent (mostly). My messages to the VDP were now landing every time.

With a fairly robust method of enabling RGB, implementing an alternative to the Bus Pirate was next on the list. A cheap Arduino Pro Micro was obtained and work begun on re-constructing the Bus Pirate I2C messages in code for that platform. A I2C library was sourced to make the sending of I2C messages trivial. Please find the arduino sketch in the ```sketch_i2c_pro_micro``` folder.

To summarise the operation of the sketch and the process for getting into RGB mode:

1) Set the pin mode A0 to digital and high - this enables the MICOM on the I2C bus at initialisation, i.e as soon as the set is turned on
2) Wait a couple of seconds for the TV set to start-up
3) Start I2C messaging, set it to slow communication, pullup resistors to true, and timeout to 100ms. These values were discovered only through trial and error.
4) Delay 15 seconds - this gives the user enough time to put the set into AV mode
5) Set pin A0 to low, stopping the MICOM on the I2C bus.
6) Delay 2 seconds - this is required to ensure the bus is as quiet as possible I guess. Again, trial and error here too.
7) Send the necessary I2C packet to enable RGB and set the correct OSD priority
8) Delay another 2 seconds to ensure the message gets through to the VDP cleanly
9) Set pin A0 to high, enabling the MICOM again
10) Profit!

To make the Arduino a permanent solution, necessary power supply voltage was provided on it's RAW pin. As this micro was a 5V model, I sourced 5V from pin 36, VSUPAB of the VDP. Not sure about the digital ground used from pin 5 and the analog power supply, but everything seems to work as expected. No fires... yet.

There is one known disadvantage to the approach taken: to enter the service mode, the set must be powered off and on again, with remote key presses of STANDBY, INFO, MENU, MUTE, STANDBY. This of course restarts the RGB enable process, kicking the user out of the service menu after 15 seconds. To disable this functionality, a constant source of power can be provided to the Arduino via USB. Users can then enter the service menu as the Arduino doesn't restart. I do wish I had a factory remote or the infrared code to access the service menu without having to force the TV into standby. Having access to the service menu this way would eliminate a major disadvantage, and perhaps allow service menu changes in the presence of an RGB image.

The Arduino sketch contains several helper functions, including some for setting FastProcessor registers. FP registers have a special interface, with multiple I2C packets often required to set bits across the usually 16 bit registers.

## Additional notes

An attempt was made to switch the VDP input to AV input automatically. The theory was that this would remove the need for the user to select the AV input prior to RGB enable. This was only partially successful. I was able to change the Video Input (VIN) to VIN4, and have the RGB signal display. However there were a few blocking issues:

1) The vertical size of the image appeared to be squished. I wondered if this was a standard selection issue (PAL/NTSC), but after disabling the ASR and setting it to NTSC standard, the bottom of the screen is still partially filled with a black box. Perhaps this could be removed via service menu changes?
2) Audio is probably missing - we have only selected video after all. Perhaps more messages would be required to control the chassis DSP audio processor.
3) Upon re-enabling the MICOM, it must detect that the selected input on the VDP does not match it's own internally selected input (usually TV). It sends messages to set the VIN back to it's internal setting, disabling our previous video input change.

It was also hoped that this might solve the problem of AV/RGB input being unavailable in the service menu. Unfortunately, this has the effect of exiting the service menu, and because the MICOM is deactivated, we can't make changes in the menu anyway.

My other thought was that it might be possible to do much of the Deflection service menu settings via I2C. For example, provide HPOS values to center the screen programatically. Unfortunately again, it appears that settings do not affect the RGB image, *BUT* do affect AV and TV inputs. I will do further investigation, but as it stands, the set is fairly usable in it's current form, albeit with some quality of life features missing.

### More additional notes and sniff dumps

Just because I didn't want to delete this stuff

// Fast processor i2c interface - writes

```
[0b10001010 0x37 address-hb address-lb]
[0b10001010 0x38 data-hb data-lb]
```

WPa[0b10001010 0x37 0x00 0x21][0b10001010 0x38 0x00 0x03]Apw
WPa[0b10001010 0x37 0x00 0x20][0b10001010 0x38 0x00 0x01]Apw
WPa[0x8E 0x30 0x00]Apw
WPa[0b10001010 0x37 0x21][0b10001010 0x38 0x03]Apw

[& 0b10001110 & 0x9f & 0b10001111 & r & r &]
[0b10001010 0x9f 0b10001011 r r]

[0b10001010 0x148 0x00]
[0b10001010 0x31 0b00001101] - RGB enable
[0b10001010 0x31 0b00000101] - RGB enable - maybe work with osd
[0b10001010 0x4b 0b10000001] - OSD testing
[0b10001010 0x9d 0b00000010] - 480i?
[0b10001010 0x10 0b10000000] - sync
						 
// RGB + OSD!
p A %:10 P %:10 ] %:10 [ %:10 0b10001010 %:10 0x31 %:10 0b00000101 %:10 0x10 %:10 0b00000000 %:10 ] %10 p %10 a

[0b10001010 0x31 0b00000101 0x10 0b00000000]
[0b10001010 0x9D 0b0000010]
[0b10001010 0x29 0x08 0x00]
[0b10001010 0x21 0b00000001] - switch input

Sniff of bus while changing to AV on the remote

+ = ack
- = nak

x80x30x0x[0x0x30x0xB[0x80x0x00x]0x80x0x80x0x80x0x00x3[0x0x30x02+0x44+][0x8E+0x30x0x]0x80x30x0x30x80x30x0xE[0x0x350x0x[0x0x30x0x30x80x30x0x8[0x80x[0x0x]0x80x0x00x[0x80x0x00x]0x80x30x80]0x80x30x0xB0x80x30x00x[0x80+0x12+0x00+0x08+0x07+0x00+][0x80+0x12+0x00+0x00+0xFF+0xE0+][0x80+0x13+0x00+0x13+[0x81+0x7D+0x40-][0x80+0x12+0x00+0x13+0x7C+0x40+][0x80+0x12+0x00+0x08+0x02+0x20+][0x80+0x12+0x00+0x41+0x02+0x20+][0x80+0x12+0x00+0x0A+0x00+0x00+][0x8A+0x67+0x01+0x31+][0x8A+0x7F+0x00+0x0D+][0x8A+0x77+0x00+0x17+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x01+0x1B+][0x8E+0x38+0xF9+0x85+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x01+0x1C+][0x8E+0x38+0x03+0x8F+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x01+0x1D+][0x8E+0x38+0x00+0xB8+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x01+0x1E+][0x8E+0x38+0xFF+0x81+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x01+0x2B+][0x8E+0x38+0xFE+0x0F+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x01+0x2C+][0x8E+0x38+0x00+0xC2+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x01+0x2D+][0x8E+0x38+0x01+0x7E+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x01+0x2E+][0x8E+0x38+0xFB+0xFC+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x01+0x2F+][0x8E+0x38+0x02+0xAD+][0x80+0x13+0x00+0x18+[0x81+0x00+0x00-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0x13+][0x8E+0x38+[0x8F+0x00+0x2A-][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0xCB+][0x8E+0x38+[0x8F+0x01+0x06-][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0x21+][0x8E+0x38+[0x8F+0x0D+0x03-][0x8E+0x35+[0x8F+0x00-]



[0x8E+0x37+0x00+0x21+][0x8E+0x38+0x0D+0x03+][0x8A+0x7F+0x00+0x0D+][0x8A+0x77+0x00+0x17+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x00+0x23+][0x8E+0x38+0x0E+0x40+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x00+0x20+][0x8E+0x38+0x00+0x21+][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0x20+][0x8E+0x38+[0x8F+0x00+0x21-][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0x20+][0x8E+0x38+[0x8F+0x00+0x21-][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0x20+][0x8E+0x38+[0x8F+0x00+0x21-][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0x20+][0x8E+0x38+[0x8F+0x00+0x21-][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0x20+][0x8E+0x38+[0x8F+0x08+0x21-][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x00+0xBE+][0x8E+0x38+0x00+0x03+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x00+0x3C+][0x8E+0x38+0x02+0x44+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x00+0x3D+][0x8E+0x38+0x00+0xE6+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x00+0x30+][0x8E+0x38+0x0E+0x88+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x00+0xDC+][0x8E+0x38+0x00+0x28+][0x80+0x13+0x00+0x13+[0x81+0x7C+0x40-][0x80+0x12+0x00+0x13+0x7C+0x40+][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0xCB+][0x8E+0x38+[0x8F+0x01+0x06-][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x00+0xAB+][0x8E+0x38+0x01+0xD1+][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0x21+][0x8E+0x38+[0x8F+0x0D+0x03-][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x00+0x21+][0x8E+0x38+0x01+0x03+][0x8E+0x35+[0x8F+0x00-][0x8E+0x37+0x00+0xBE+][0x8E+0x38+0x00+0x03+][0x8A+0x7C+0x01+0xF7+][0x8A+0x74+0x00+0x8C+][0x8A+0x6C+0x01+0xEC+][0x8A+0x64+0x01+0xCE+][0x8A+0x5C+0x00+0x7C+][0x8A+0x54+0x00+0x00+][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0x13+][0x8E+0x38+[0x8F+0x00+0x2A-][0x8E+0x35+[0x8F+0x00-][0x8E+0x36+0x00+0x12+][0x8E+0x38+[0x8F+0x02+0x04-][0x80+0x13+0x00+0x0E+[0x81+0x40+0x00-][0x80+0x13+0x00+0x10+[0x81+0x50+0x00-][0x80+0x13+0x00+0x0D+[0x81+0x3B+0x00-][0x80+0x13+0x00+0x07+[0x81+0x6E+0x01-][0x80+0x10+0x00+0xBB+0x00+0xD0+][0x80+0x10+0x00+0x83+0x82+0x64+][0x80+0x10+0x00+0x01+0x00+0x56+][0x80+0x10+0x00+0x01+0x00+0x32+][0x80+0x10+0x00+0x01+0x00+0x0A+][0x80+0x10+0x00+0x01+0x00+0xF6+][0x80+0x10+0x00+0x01+0x00+0xF8+][0x80+0x10+0x00+0x01+0x00+0xFE+][0x80+0x10+0x00+0x93+0x00+0x00+][0x80+0x10+0x00+0x9B+0x05+0x14+][0x80+0x10+0x00+0xAB+0x04+0xC6+][0x80+0x10+0x00+0xA3+0x03+0x8E+][0x80+0x11+0x00+0x23+[0x81+0x00+0x00-][0x80+0x13+0x00+0x13+[0x81+0x7C+0x40-][0x80+0x12+0x00+0x13+0x7C+0x40+][0x80+0x12+0x0x00x0x00x80x0x00x00x0x20x80x0x00x0x00x]0x80x0x00x0x00x00x80x30x80x[0x0x30x1[0x80+0x11+0x00+0x23+[0x81+0x00+0x00-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x90-][0x80x0x80x0x80x0x00x]0x80x0x80x0x[0x0x350x80]0x0x30x00x[0x0x30x80x0]0x80x0x00x[0x0x0x[0x0x10x0x10x0x0x]0x0x10x00x0x80x0x0x80x0x00x00x0x0x]0x80x0x00x[0x0x0x[0x0x10x0x10x80x0x0x80x0x00x10x0x40x80x0x00x00x0x00x80x0x00x0x00x]0x80x12+0x00+0x41+0x00x]0x80x0x00x0x00x][0x80+0x110x0x20x80x0x0x90x9[0x[0x[0x[0x]0x]0x]0x]0x]0x90x90x90x90x90x90x90x9[0x[0x[0x80x30x0x[0x0x30x0x1[0x0x30x0x0x]0x80x[0x0x0x80x30x0x10x80x[0x0x0x[0x0x10x0x00x80x0x0x80x0x00x0x80x0x[0x0x10x0x00x0x0x]0x0x10x00x0x80x0x[0x0x10x0x20x80x0x]0x80x0x00x[0x0x0x[0x0x10x0x10x70x[0x0x10x0x00x0x0[0x0x10x0x00x0x2[0x0x10x0x40x0x2[0x0x10x0x00x0x0[0x80+0x10x00x[0x0x0x[0x]0x]0x90x90x90x90x90x90x90x9[0x]0x]0x]0x]0x]0x]0x]0x90x90x9[0x80x0x80x0x80x0x00x10x80x0x80x0x[0x0x30x8F0]0x0x30x00x[0x0x30x80x0x0x80x0x00x[0x0x0x[0x0x10x0x10x0x50x0x0x10x00x0x80x0x0x80x10x0x00x0x0x]0x80x0x00x[0x0x0x[0x0x10x0x10x80x0x0x80x10x0x10x0x40x80x10x0x00x0x00x80x0x00x0x00x]0x80x0x00x0x00x]0x80x0x00x0A+0x00+0x00+][0x80+0x110x0x20x80x0x0x9[0x[0x[0x[0x]0x]0x]0x]0x]0x]0x90x90x90x90x90x9[0x[0x[0x[0x]0x80x30x0x[0x0x30x0x1[0x0x30x0x0x]0x80x[0x0x0x80x30x0x10x80x[0x0x0x[0x0x30x80]0x80x0x00x]0x80x0x00x[0x0x30x80]0x80x0x00x]0x0x30x00x[0x0x30x80]0x80x0x00x][0x0x30x00x[0x80+0x13+0x00+0x13+0x80x0x[0x0x10x0x10x0x4[0x0x30x0x]0x0x30x0xC[0x0x30x0x0x]0x80x30x0x[0x0x30x0xB[0x0x30x0x0[0x0x30x80]0x0x30x00x[0x0x30x80x0x0x80x[0x0x[0x0x30x0x20x80x0x00x0[0x0x70x0x0[0x0x70x0x1[0x8E+0x35+[0x8F+0x00-0x80x0x00x]0x80x0x00x[0x0x30x80]0x80x0x00x]0x0x30x00x[0x0x30x80]0x80x0x00x]0x0x30x80x0x[0x0x30x0x[0x0x30x0x2[0x0x30x0x0x]0x80x[0x0x[0x0x30x0x20x80x[0x0x0x[0x0x30x80x0x80x0x00x]0x0x30x80x0x[0x0x30x80]0x0x30x00x[0x0x30x0x0x][0x8E+0x35+[0x8F+0x[0x0x30x0x20x80x[0x0x0x[0x0x30x80]0x80x0x00x]0x0x30x00x[0x0x30x80]0x80x0x00x]0x0x30x00x[0x0x30x80]0x80x0x00x]0x0x30x00x[0x0x30x80]0x80x0

Breakdown of sniffed traffic, looking for writes:

[0x8A+0x67+0x01+0x31+] - Vertical blanking start
[0x8A+0x7F+0x00+0x0D+] - Tube measurement line
[0x8A+0x77+0x00+0x17+] - Vertical blanking stop
[0x8A+0x7C+0x01+0xF7+] - Inv matrix co
[0x8A+0x74+0x00+0x8C+] - Inv matrix co
[0x8A+0x6C+0x01+0xEC+] - Inv matrix co
[0x8A+0x64+0x01+0xCE+] - Inv matrix co
[0x8A+0x5C+0x00+0x7C+] - Inv matrix co
[0x8A+0x54+0x00+0x00+] - Inv matrix co


[0x8E+0x37+0x00+0x21+][0x8E+0x38+0x0D+0x03+]
[0x8E+0x37+0x00+0x23+][0x8E+0x38+0x0E+0x40+]
[0x8E+0x37+0x01+0x1B+][0x8E+0x38+0xF9+0x85+]
[0x8E+0x37+0x00+0xBE+][0x8E+0x38+0x00+0x03+]
[0x8E+0x37+0x00+0x21+][0x8E+0x38+0x01+0x03+]
[0x8E+0x37+0x00+0xAB+][0x8E+0x38+0x01+0xD1+]
[0x8E+0x37+0x00+0xDC+][0x8E+0x38+0x00+0x28+]