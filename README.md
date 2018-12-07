Create influxDB
`curl -POST http://localhost:8086/query --data-urlencode "q=CREATE DATABASE yutaki"`

https://www.domoticz.com/forum/viewtopic.php?f=5&t=4072&start=20
http://www.domoticz.com/wiki/Evohome

sudo modprobe ti_usb_3410_5052
echo 10ac 0102 | sudo tee /sys/bus/usb-serial/drivers/ti_usb_3410_5052_1/new_id

sudo minicom -b 115200 -D /dev/ttyUSB0


