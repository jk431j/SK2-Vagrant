# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure("2") do |config|
  config.vm.box = "bento/ubuntu-16.04"

  config.vm.define "sk2sdk"
  config.vm.hostname = "sk2sdk"

  config.vm.box_check_update = false

  config.vm.provider "virtualbox" do |vb|
	vb.customize ["modifyvm", :id, "--usb", "on"]
	vb.customize ["modifyvm", :id, "--usbehci", "on"]
	vb.customize ['usbfilter', 'add', '0', '--target', :id, '--name', 'SK2', '--vendorid', '0x1435', '--productid', '0x3182']

	vb.customize ['guestproperty', 'set', :id, '/VirtualBox/GuestAdd/VBoxService/--timesync-interval', 10000]
	vb.customize ['guestproperty', 'set', :id, '/VirtualBox/GuestAdd/VBoxService/--timesync-min-adjust', 100]
	vb.customize ["guestproperty", "set", :id, "/VirtualBox/GuestAdd/VBoxService/--timesync-set-on-restore", "1" ] 
	vb.customize ['guestproperty', 'set', :id, '/VirtualBox/GuestAdd/VBoxService/--timesync-set-threshold', 1000]
  end

  config.vm.provider "vmware_desktop" do |v, override|
	override.vm.hostname = "sk2sdkvmx"
	v.vmx["usb.present"]  = "true"
	v.vmx["usb.autoConnect.device0"]  = "0x1435:0x3182"
  end

  config.vm.provision "shell", path: "scripts/provision.sh"
end
