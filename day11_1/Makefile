TARGET = gpio_workqueue
obj-m = $(TARGET).o

modules :
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C /home/parth/Desktop/linux M=`pwd` modules

clean : 
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C /home/parth/Desktop/linux M=`pwd` clean

copy : 
	scp `pwd`/$(TARGET).ko debian@192.168.7.2:/home/debian/parth
	
.phony : modules clean copy
