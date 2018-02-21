make TARGET=xm1000 savetarget
make sensorReading
sudo make sensorReading.upload

echo "running your application"
#sudo ../../tools/sky/serialdump-linux -b115200 /dev/ttyUSB0
sudo make login

