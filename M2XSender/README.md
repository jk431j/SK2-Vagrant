# M2X data sender

StarterKit application for periodically sending data to M2X. 

You should already have Vagrant virtual machine installed (see top level README.md). The application will be compiled during installation.

1. Ssh to Vagrant machine: `vagrant ssh`

2. Push compiled application into `/CUSTAPP ` directory on the StarterKit:
```shell
vagrant@sk2sdk:~$ adb push ~/M18QxIotMonitor/m2x_sender /CUSTAPP
565 KB/s (98820 bytes in 0.170s)
```

3. Open shell on StarterKit. If `iot_monitor` is still running terminate it first:
```shell
vagrant@sk2sdk:~$ adb shell
/ # ps -A | grep iot
 3396 ?        00:07:50 iot_monitor
/ # kill 3396
```

4. Start m2x_sender application. You have to specify two parameters:
- Interval how often are data being sent in seconds
- M2X API key. Use master API key, that way the application can create the device if it does not exist yet.
```shell
/ # /CUSTAPP/m2x_sender 5 your_M2X_master_API_key
Using ICCID 12345678901234567890 as M2X device serial
-Validating API Key and Device ID...
Create a new device.
Attempting to create device with id 12345678901234567890
-Creating the data streams...
Using API Key = beefbeefbeefbeefbeefbeefbeefbeef, Device Key = cafecafecafecafecafecafecafecafe
LED colors will display a different colors after each set of sensor data is sent to M2X.

To exit the Quick Start Application, press the User Button on the Global 
LTE IoT Starter Kit for > 3 seconds.

 1. Sending ADC value, TEMP value, XYZ values...All Values sent. (delay 1 seconds)
```

5. If everything is configured properly you should see new device being created in M2X and data being sent into device streams. You can stop the application by holding USER (middle) button for 3 seconds.

6. Now it's time to setup the application to be started on reboot. Still in StarterKit shell type following:
```shell
/ # cd /CUSTAPP/
/CUSTAPP # cp custapp-postinit.sh custapp-postinit.sh.old
/CUSTAPP # echo "start-stop-daemon -S -b -x /CUSTAPP/m2x_sender -- 60 your_M2X_master_API_key" > custapp-postinit.sh
/CUSTAPP # 
```

7. Exit back to Vagrant machine and reboot StarterKit. After the reboot StarterKit will start sending data to M2X on the background:
```shell
/CUSTAPP # exit
vagrant@sk2sdk:~$ adb reboot
```