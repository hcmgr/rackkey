# rackkey
---
A distributed key value store for arbitrary data types, including files, blobs, objects and in-memory data structures.

### Key features
- Distributed storage: Files are divided into blocks and distributed across a cluster of storage nodes.
- Horizontal scalability: Easily scale your storage infrastructure by adding more nodes. Dockerized nodes and dynamic rebalancing ensures adding/removing nodes is easy as pie.
- Efficient retreival: Utilizes indexation and a read-optimised on-disk format for fast data retreival.
- Replication: Blocks are replicated across a configurable number of storage nodes, ensuring fault tolerance.
- Monitoring: Node statistics and health checks makes it easy to monitor the  state of the storage cluster.

### Supports:
- multiple machines
- replication
- indexation 
- dynamic rebalancing
- node monitoring and statistics

### API
##### `/store/{KEY}`
- GET/PUT/DELETE
    - read/write/delete data for given `KEY`

##### `/keys`
- GET
    - retreive all keys stored by the storage cluster

##### `/stats`
- GET
    - retreive stats on the storage cluster usage

### Configurability
Both master and storage node functionality is configurable using `src/config.json` (see src/ for detailed instructions on how to use `config.json`)

### Install

##### Master
```bash
sudo apt update
sudo apt-get install g++ cmake ## build
sudo apt-get install libssl-dev libccprest-dev ## core libraries used

sudo apt-get install unzip ## testing purposes
```

##### Storage
```bash
sudo apt update
sudo apt-get install g++ cmake ## build
sudo apt-get install libssl-dev libccprest-dev ## core libraries used

## install docker
sudo apt update
sudo apt install apt-transport-https ca-certificates curl software-properties-common
sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"
sudo apt install docker-ce
sudo systemctl status docker
docker --version

## install docker-compose
sudo curl -L "https://github.com/docker/compose/releases/download/v2.21.0/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose
docker-compose --version
```

### Start services
##### Master
```bash
mkdir build
cd build
cmake ..
make
./master
```

##### Storage
```bash
mkdir ~/.rackkey
cd src/storage/docker
./scripts/deploy.sh
```

### Usage
```bash
curl -X PUT localhost:9000/store/images.zip --data-binary @in/images.zip
curl -X GET localhost:9000/store/images.zip -o out/images.zip
```
