FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    libcpprest-dev \
    libssl-dev \
    vim \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# copy storage AND shared code
COPY ./storage/CMakeLists.txt /app/
COPY ./storage /app/storage
COPY ./shared /app/shared
COPY ./config.json /app/

RUN mkdir build && cd build && cmake .. && make

EXPOSE 8080

CMD ["/app/build/storage"]

