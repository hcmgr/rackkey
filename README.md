Distributed key value store in C++ (work in progress)

### Required
C++17

### Install: Master and Storage
sudo apt update
sudo apt install g++
sudo apt install cmake
sudo apt install libssl-dev
sudo apt-get install libcpprest-dev

## Install: Storage-specific

##### docker
sudo apt update
sudo apt install apt-transport-https ca-certificates curl software-properties-common
sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"
sudo apt install docker-ce
sudo systemctl status docker
docker --version

##### docker-compose
sudo curl -L "https://github.com/docker/compose/releases/download/v2.21.0/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose
docker-compose --version

## Other
sudo apt install unzip

## Steps: Master
mkdir build
cd build
cmake ..
make
./master

## Steps: Storage
mkdir ~/.rackkey
cd src/storage/docker
./scripts/deploy.sh

## Client usage
