# HD-WF1-LED-MatrixPanel-DMA
 Custom firmware based on my [HUB75 DMA library](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA),  for the Huidu WF1 LED Controller Card based on the ESP32-S2.

 These can be bought for about USD $5 and are quite good as they come with a battery and a Real Time Clock (BM8563), and a push button connected to GPIO 11 for your use.
 
[ https://www.aliexpress.com/item/1005005038544582.html ](https://www.aliexpress.com/item/1005006075952980.html)

![image](https://github.com/mrfaptastic/HD-WF1-LED-MatrixPanel-DMA/assets/12006953/ccdff75b-b764-424a-b923-dbac86f1b151)

 
 ## How to flash?
 
To put the board into Download mode, simply bridge the 2 pads near the MicroUSB port, then connect the board into your PC using a USB-A to USB-A cable, connecting to the **USB-A port** on the device (not the Micro USB input). It is NOT possible to program the board using the onboard Micro USB port. Thanks [Rappbledor](https://github.com/mrfaptastic/HD-WF1-LED-MatrixPanel-DMA/issues/3)

![image](https://github.com/mrfaptastic/HD-WF1-LED-MatrixPanel-DMA/assets/12006953/adddb545-856e-4d61-b4ab-a88a39814969)


Alternatively, solder a micro-usb to the exposed pins underneath to gain access to the D+ and D- lines connected to the USB-A port on the device. The ESP32-S2 routes the USB D+ and D- signals to GPIOs 20 and 19 respectively. 

![image](https://github.com/mrfaptastic/HD-WF1-LED-MatrixPanel-DMA/assets/12006953/fba33a4d-9737-4366-9a3b-776bec22ab2f)

![image](https://github.com/mrfaptastic/HD-WF1-LED-MatrixPanel-DMA/assets/12006953/9b8b4b9a-89b9-4707-8c9a-8e2cb30d4852)


The above has been successfully tested on board is rev 7.0.1-1 and it comes with an ESP32-S2 with 4MB flash and no PS Ram.

## GitHub Sketch Output
It should show a clock and some colourful graphics. Designed for a 64x32px LED matrix.

![image](https://github.com/user-attachments/assets/d7293beb-f293-4741-9fbf-6555be1db297)

## Huidu WF2
I don't have a Huidu WF2, but others have noted it's an ESP32-S3 based and works as well. Refer to these links for more information or configuration settings:
* https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/issues/433
* https://github.com/hn/esphome-configs

