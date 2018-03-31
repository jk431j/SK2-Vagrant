#!/bin/bash

PACKAGES='git libtool automake adb ntpdate'
AVNET_BASE=https://github.com/Avnet
WNC_SDK=${AVNET_BASE}/AvnetWNCSDK
IOT_MON=M18QxIotMonitor
OECORE_ENV=/usr/local/oecore-x86_64/environment-setup-cortexa7-neon-vfpv4-oe-linux-gnueabi
UDEV_RULES=/etc/udev/rules.d/51-android.rules

if [[ $HOSTNAME == "sk2sdk" ]]
then
	WORKDIR=/vagrant
elif [[ $HOSTNAME == "sk2sdkvmx" ]]
then
	WORKDIR=/home/vagrant
else
	echo This script is run from Vagrant, do \"vagrant up\" or \"vagrant provision\" instead
	exit 1
fi

apt-get update
apt-get -y install $PACKAGES
if [[ $? == 100 ]] 
then
	echo ======================================
	echo Package installation failed.
	echo Check your network connection and
	echo rerun provisioning by typing:
	echo 
	echo    vagrant provision
	echo ======================================
	exit 2
fi

echo Syncing time to NTP server
ntpdate ntp.ubuntu.com

if [ -f $UDEV_RULES ]
then
	echo UDEV rules already installed in $UDEV_RULES
else
	echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="1435", MODE="0666", GROUP="plugdev"' > $UDEV_RULES
	chmod a+r $UDEV_RULES
	udevadm control --reload-rules
	udevadm trigger
fi

if [ -f $OECORE_ENV ]
then
	echo WNC SDK already installed.
else
	cd $WORKDIR
	git clone $WNC_SDK
	AvnetWNCSDK/oecore-x86_64-cortexa7-neon-vfpv4-toolchain-nodistro.0.sh -y
fi

sudo -EHu vagrant bash <<SUDO
if [ -f ~/.android/adbkey.pub ] 
then
	echo ADB already configured
else
	mkdir ~/.android
	cp ${WORKDIR}/AvnetWNCSDK/adbkey.pub ~/.android/
	touch ~/.android/adbkey
fi

if ! grep -q "$OECORE_ENV" ~/.bashrc
then
	echo ". $OECORE_ENV" >> ~/.bashrc
fi
. $OECORE_ENV

cd $WORKDIR
if [ -d $IOT_MON ]
then 
	echo $IOT_MON already exists, skipping straight to make
	cd $IOT_MON
else
	git clone ${AVNET_BASE}/${IOT_MON}
	ln -s ${WORKDIR}/${IOT_MON}/ ~ 2>/dev/null
	cd $IOT_MON
	LC_ALL=C ./autogen.sh 2>/dev/null
	./configure \$CONFIGURE_FLAGS
fi

make
SUDO
