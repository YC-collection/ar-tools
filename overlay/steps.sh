# http://derekmolloy.ie/gpios-on-the-beaglebone-black-using-device-tree-overlays/


cd /home/machinekit/proj/ar-tools/overlay
./build


sudo su -
export SLOTS=/sys/devices/bone_capemgr.9/slots
export PINS=/sys/kernel/debug/pinctrl/44e10800.pinmux/pins
cp AR11-00A0.dtbo /lib/firmware
cat $SLOTS
echo AR11 > $SLOTS
# echo -7 > $SLOTS # remove 8th overlay from device tree
cat $SLOTS

ls -al /dev/spidev1.*
cat /sys/kernel/debug/pinctrl/44e10800.pinmux/pingroups
    # group: pinmux_fpga_config_pins
    # pin 28 (44e10870)
    # pin 29 (44e10874)
    # pin 16 (44e10840)
    # group: pinmux_spi1_pins
    # pin 100 (44e10990)
    # pin 101 (44e10994)
    # pin 102 (44e10998)
    # pin 103 (44e1099c)


cat $PINS | grep "870"  # CFG_PROGB, OUT, GPIO_30 in .dts file
    # should be: OUTPUT w/ PULLUP
    # pin 30 (44e10878) 00000017 pinctrl-single 
cat $PINS | grep "874"  # CFG_INITB, IN,  GPIO_31 in .dts file
cat $PINS | grep "840"  # CFG_DONE,  IN,  GPIO_48 in .dts file
    # should be: INPUT w/ PULLUP
    # pin 29 (44e10874) 00000037 pinctrl-single 
    # pin 16 (44e10840) 00000037 pinctrl-single 

ID=30   # CFG_PROGB
echo $ID > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio${ID}/direction
echo 1 > /sys/class/gpio/gpio${ID}/value      # voltage: 3.0 V
echo 0 > /sys/class/gpio/gpio${ID}/value      # voltage: 0   V

ID=31   # CFG_INITB
ID=48   # CFG_DONE
echo $ID > /sys/class/gpio/export
echo in > /sys/class/gpio/gpio${ID}/direction
cat /sys/class/gpio/gpio${ID}/value
