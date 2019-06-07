# General Information
This sketch was a little side project to control my Pentair variable speed pool pump. I used an Afero development board for the connectivity.

The final assembly was supposed to live in a waterproof case and be powered by an 9V battery. The hardward I used is a BTLE module that uses your mobile phone as the internet gateway. The Afero mobile app has a built-in hub for Bluetooth devices. You can of course also use the WiFi developer board Modulo-2.

Although I never completed the project, here is a short video of how the pump control would have work: https://www.youtube.com/watch?v=T3iz8tXQJtM

The application code has minimal integration with Afero, so it should be relatively easy to replace it with another IoT solution.

 # Libraries
* Timer.h: https://github.com/JChristensen/Timer/archive/master.zip. I am sure you can probably use also Arduino-Timer, just need to change the include to lowercase timer.h. But I haven't tested this.
* afLib3: https://github.com/aferodeveloper/afLib3. This is what enables the communication with the Afero Cloud.

The libraries need to be placed into your Arduino home folder. On macOS Arduiono home is located at `~/Documents/Arduino/libraries` respectively at `%userprofile%\Documents\Arduino\Libraries`. If the `libraries` subfolder does not exist, just create it and then drop the aforementioned library folders in there. The IDE picks them up automatically.

# Hardware
I used a Modulo-1 from Afero to facilitate the communication with the cloud. Other then that, I used an AT Mega2560 and an Arduino-compatible RS485 for the serial communication with the pump. Below a few links to the hardware:
* Modulo-1: https://www.mouser.com/ProductDetail/Afero/AFERO-DB-01?qs=sGAEpiMZZMvt1VFuCspEMiqkvWLVfJyZ7ubOxitdVJ8%3D
* AT Mega2560: https://store.arduino.cc/usa/mega-2560-r3
* RS485 module: https://www.amazon.com/gp/product/B00NIOLNAG

Optional hardware for "production" use:
* Waterproof case _(optional)_: https://www.amazon.com/Waterproof-Case-Pelican-1040-Micro/dp/B001GGBORU
* Male DC power plug (_optional_): https://www.amazon.com/gp/product/B016EUAWJ8
* 9V battery (_optional_): Available at your local battery dealer!
* Arduino Uno (_optional, It's more compact and may use less power than the Mega. Not sure about the power._): https://www.amazon.com/gp/product/B00CBZ4CII
 

# Other Links
* Afero Developer Portal: https://developer.afero.io
* Afero Community Forum: https://forum.afero.io
* Standalone hub: https://developer.afero.io/StandaloneHub