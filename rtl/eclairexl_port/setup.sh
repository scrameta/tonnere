rm a8core components zpu usbhostslave -rf
ln -s ../../atari_800xl/common/rtl/a8core
ln -s ../../atari_800xl/common/rtl/components
ln -s ../../atari_800xl/common/rtl/zpu
mkdir usbhostslave
cp ./components/usbhostslave/trunk/RTL/*/* usbhostslave/

