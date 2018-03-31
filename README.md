# SK2-Vagrant
Vagrant files for Starter Kit 2 development

1. Install [Vagrant](https://www.vagrantup.com)
2. Install [VirtualBox](https://www.virtualbox.org)
3. Clone this repo
4. Go to the cloned directory (should be *SK2-Vagrant*)
5. Run `vagrant up`
6. Vagrant does its magic and you should end up with compiled `iot_monitor` app in `M18QxIotMonitor` subdirectory
7. Virtual machine is still running, you can access it with `vagrant ssh`, stop with `vagrant halt` or completely remove with `vagrant destroy`

StarterKit USB device is now mapped to the VM. If you want to access the StarterKit do the following and you will end up in the root shell on the StarterKit.:
```shell
$ vagrant ssh
Welcome to Ubuntu 16.04.4 LTS (GNU/Linux 4.4.0-87-generic x86_64)

vagrant@sk2sdk:~$ adb devices
List of devices attached
WNC_ADB	device

vagrant@sk2sdk:~$ adb shell
/ # 
```

You can use `adb push` to copy the compiled code to the device:
```shell
vagrant@sk2sdk:~$ adb push M18QxIotMonitor/iot_monitor /CUSTAPP/test
542 KB/s (103348 bytes in 0.186s)
vagrant@sk2sdk:~$ 
```
