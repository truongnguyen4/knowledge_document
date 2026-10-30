#!/system/bin/sh
G=/config/usb_gadget/g1
# sys.usb.controller is dummy_udc.0 on this device; the real UDC is in vendor.usb.controller
UDC=$(getprop vendor.usb.controller)
[ -z "$UDC" ] && UDC=$(ls /sys/class/udc | grep -v dummy | head -1)
echo "UDC=$UDC"

echo "" > $G/UDC
[ -d $G/functions/hidmouse.0 ] || mkdir $G/functions/hidmouse.0 || echo "mkdir failed"
[ -L $G/configs/b.1/f_hidmouse ] || ln -s $G/functions/hidmouse.0 $G/configs/b.1/f_hidmouse || echo "ln failed"
echo "links : $(ls $G/configs/b.1 | tr '\n' ' ')"
echo $UDC > $G/UDC
sleep 2
echo "UDC   : $(cat $G/UDC)"
echo "state : $(cat /sys/class/udc/$UDC/state)"
dmesg | grep -iE "hidmouse|f_hidmouse" | tail -20
