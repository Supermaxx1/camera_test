# camera_test

sudo apt update
sudo apt full-upgrade -y
reboot


wget https://github.com/prepkg/opencv-raspberrypi/releases/latest/download/opencv_64.deb
sudo apt install -y ./opencv_64.deb
rm opencv_64.deb
sudo apt install -y g++

g++ main.cpp -o test `pkg-config --cflags --libs opencv4`
./test




