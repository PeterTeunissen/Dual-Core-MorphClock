# Dual-Core-MorphingClock

![alt text](https://github.com/PeterTeunissen/Dual-Core-MorphClock/blob/master/dual-core-morphclock.jpg?raw=true)

The clock code is based on https://github.com/lmirel/MorphingClockRemix.git. 

Now it is a remix of a remix... My project started when I saw HarryFun's great Morphing Digital Clock idea https://www.instructables.com/id/Morphing-Digital-Clock/
Then I found a project that was based on HarryFun's project, which added weather information, it added animated weather icons and a dimming option between 8pm and 8am. All very cool.

I wanted to also show my room temperature by using a DS1820 sensor, but the Wemos D1 Mini was out of pins, except for the Rx,Tx lines. Since I had an additional D1 Mini, I decided to use that to read the DS1820 
temperature and use that to send the value to the Wemos that runs the clock code.

Additionally, since the clock can be set to Celcius and Fahrenheit, I added another message from the clock to the temperature Wemos to tell it what units should be used. As a 'protocol' I decided to use the ```>``` and ```<``` characters 
as begin-of-message and end-of-message so that it stays readable and can be mixed with other serial (debug) information. The clock will send ```>F<``` or ```>C<``` to the temperature 
module. The temperature unit reports the temperature as ```>23<```. 

The two Wemos modules are connected together by connecting Tx to Rx and Rx to Tx lines. To program either of the two Wemos' you will need to disconnect these serial connections.

Another option I added was that if the clock is running Not in Military mode (12 hour clock), I added AM/PM indicating, and 10th-hour digit suppression when that digit is zero.

I still want to add a sync to find the UTC offset depending on daylight savings. Hopefully by using a timezone API.

- main code is based on the NTPClient lib example for ESP, the lib itself is used as is for NTP sync https://github.com/2dom/NtpClient
- use fast NTP sync on start then adapt to one sync per day (or so)
- 12/24h time format, and shows AM/PM indicator when running in 12h format.
- date format and display below the clock
- Morphing clock code/logic is kept almost as is from https://github.com/hwiguna/HariFun_166_Morphing_Clock
- WiFiManager code/logic is also from https://github.com/hwiguna/HariFun_166_Morphing_Clock - the password for connecting to ESP AP is: MorphClk
- it uses TinyFont and TinyIcons as my own implementation
- it uses openweathermap.org so you'll need a free account for the weather data (sync every 5min or so)
  !! you'll need to update the apiKey and location variables (around line 300)
- clock shows min and max temperature in lower left and right corners.
- metric or imperial units support for weather data display above the clock
- it can use animated icons for weather: sunny, cloudy, rainy, thunders, snow, etc.. (not all tested yet)
- it has night mode from 8pm to 8am when it only shows a moon and 2 twinkling stars and a dimmed display
- temperature and humidity change color based on (what most might consider) comfortable values

[Note]: In case you have issues running the WiFi configuration applet, disable ICONS usage.
//#define USE_ICONS
//#define USE_FIREWORKS
//#define USE_WEATHER_ANI
You can re-enable it after configuration is done.
Alternatively, you can use the 'no-wm' branch for a static configuration approach.

Tested with two Wemos D1 Mini Pro, running at 160MHz and the PxMatrix code from Jan 19, 2019.

Provided 'AS IS', use at your own risk

TODOs:

- [ ] Get time zone information. Hopefully the Master has room to do this.
- [ ] Change AM/PM colors
- [ ] Make sure AM/PM colors also dim
- [ ] Do we remove the temperature and humidiy colors?
- [ ] Presure color not pretty
- [ ] RGB Led Strip on the back of box. Add wifi to this one to allow phone app drive the led strip color.
- [ ] Add wifi manager to Slave
- [ ] Add config mode during startup when a button is pressed.