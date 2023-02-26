# D5-Smartmeter

https://www.notion.so/chronohax/How-to-Run-D5-Smartmeter-using-avrdude-5ddb180966ca43e4a3ef07abf9784f8a

# How to Run D5 Smartmeter using avrdude

# 1. make sure you have avrdude and avr-gcc installed

Either:

1. use the university computer on the lab or remote desktop via Horizon VMware 
or 
1. Install avrdude [GitHub - avrdudes/avrdude: AVRDUDE is a utility to program AVR microcontrollers](https://github.com/avrdudes/avrdude/) by adding it to Path environment variables, tutorial: [Upload Hex file on Arduino using AVRDUDE (Programming AVR Boards) (adduino.com)](https://adduino.com/upload-hex-file-on-arduino-using-avrdude-programming-avr-boards/)

1. install avr 8-bit GNU toolchain by adding it to Path env var like step 1:[GCC Compilers for AVR® and Arm®-Based MCUs and MPUs | Microchip Technology](https://www.microchip.com/en-us/tools-resources/develop/microchip-studio/gcc-compilers)

# 2. Download relevant files from Discord/here/git for latest version

Relevant constant files are:

[lcd.h](https://www.notion.so/chronohax/How-to-Run-D5-Smartmeter-using-avrdude-5ddb180966ca43e4a3ef07abf9784f8a/lcd.h)

[lcdlib.rar](https://www.notion.so/chronohax/How-to-Run-D5-Smartmeter-using-avrdude-5ddb180966ca43e4a3ef07abf9784f8a/lcdlib.rar)

updated files:

[prog.c](https://www.notion.so/chronohax/How-to-Run-D5-Smartmeter-using-avrdude-5ddb180966ca43e4a3ef07abf9784f8a/prog.c)

extract the .rar (zipped) files with winrar ~~and put prog.c into a folder named anything u want (i named mine main)~~  nvm i put them all like this

![Untitled](https://user-images.githubusercontent.com/126186721/221416897-8bed0746-6de1-4941-a9fc-48c20303ee3b.png)

# 3. Run the following commands

run cmd on the working directory, main folder in my case. by cd into it or typing cmd on search directory and pressing enter. Then, run these commands:

```bash
avr-gcc -mmcu=atmega644p -DF_CPU=12000000 -Wall -Os prog.c -o prog.elf -L. -llcd
```

```bas[Export-9b64550b-a593-4a5d-afe7-367550469728.zip](https://github.com/ChaJoon/D5-Smartmeter/files/10833910/Export-9b64550b-a593-4a5d-afe7-367550469728.zip)
h
avr-gcc -mmcu=atmega644p -DF_CPU=12000000 -Wall -Os prog.c -o prog.elf -L../liblcd -llcd
```

-L. is to link the lcd library inside liblcd folder, which sits in current directory thus the . (dot) or -L../lcdlib if the lcdlib on another folder on top directory CHECK THE NAME OF THE FOLDER WHETHER ITS LIBLCD OR LCDLIB

IF FILE/FOLDER NOT FOUND THEN MOST LIKELY THE FOLDER IS IN WRONG HIERARCHY!!!

```bash
avr-objcopy -O ihex prog.elf prog.hex
```

```bash
avrdude -c usbasp -p m644p -U flash:w:prog.hex
```

If u are getting this error on windows after running last line, u need to install the driver for usbasp using zadig[Zadig - USB driver installation made easy (akeo.ie)](https://zadig.akeo.ie/)

![Untitled 1](https://user-images.githubusercontent.com/126186721/221416916-096fe709-143c-4035-b62e-0baa53a64198.png)

![Untitled 2](https://user-images.githubusercontent.com/126186721/221416930-d3ea0cb9-0ba5-4598-8853-0e6a54b988ab.png)

Make sure the current driver is libusb and not WinUSB! it only shows up when ready to be programmed (green D1 LED status lights up)

# 4. When remaking the file for changes in liblcd.a such as changing PORTA & C to PORTB & D, make sure to delete all instance of .o and .lst files or else it wont update

Also, when programming, remove the lcd from PORTB and PORTD, then turn it off and on again if its not working! This happens too often that led to many wasted hours for me trying to debug it…

Changing port code in avrlcd.h : 

![Untitled 3](https://user-images.githubusercontent.com/126186721/221416935-2abebce2-0f3f-4158-95de-e1f6f480c2e5.png)
