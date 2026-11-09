# ESP-CAM_Vintage_Camera
Develop a Vintage-Type Camera with an ESP-CAM, including a TFT screen to see the camera view, and display your gallery

The main materials to use are an ESP-CAM, a 1.8" TFT display (although I am using a ST7735 any model should work with the correct library and pinout), a micro SD Card
to store the photos, a pair of push buttons, and the BMS and battery of your election, that can throw 5V constantly (or 3.3V if you feed it via that pin)


<img width="1122" height="610" alt="image" src="https://github.com/user-attachments/assets/6290920b-ba21-4ffc-80cc-52a0ca38171c" />

The camera works in two different Modes: Live and Gallery.
In Live mode you can take pictures by pressing the button connected to IO1, you may want to keep it press for a second to ensure a correct reading
When entering gallery mode it displays the newest to the oldest photos stored, with a blue rectangle to indicate it's in gallery mode
if you keep pressing the gallery button (IO3) you will scroll through the pictures in the order mentioned

To get back to Live Mode just press the button in IO1, again you may want to press it for a second to ensure a good reading

Disclaimer: Due to the SPI, camera and SD card pins being ocupied, the CLK pin to the TFT display is the same as the flashlight, I recommend you to
cover it with the enclosure or remove it if you don't feel like needing it
Also, my ESPCAM model had the USB communication already in the board, but they normally need an adapter to communicate with your PC

Here are the resulting pictures!

<img width="640" height="480" alt="image" src="https://github.com/user-attachments/assets/4d19988e-cfd3-459b-a9e7-f6017c06cef2" />
<img width="640" height="480" alt="image" src="https://github.com/user-attachments/assets/3b1d195e-693d-4e24-b07a-4bdcbbd13509" />
<img width="640" height="480" alt="image" src="https://github.com/user-attachments/assets/00bb7756-ccab-43ab-aaa4-5e0b6e835f97" />

Since I didn't make a proper PCB and this was Kind of a last minute idea to project to prototype, my case wasn't the best
I will try to upload and update with a V2.0
