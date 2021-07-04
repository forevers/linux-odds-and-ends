# Dockerfile build:
#   - $ docker build -t ess/rpi4-x-tools .s
# usage: 
#   - $ cd <rpi4 linux dir>
#   - $ docker run -ti -v $PWD:/build ess/rpi4-x-tools

FROM ubuntu:18.04

RUN apt-get update && apt-get install -y -q \
 git \
 build-essential \
 bc \
 bison \
 flex \
 libssl-dev \
 make \
 libc6-dev \
 libncurses5-dev \
 crossbuild-essential-armhf \
 crossbuild-essential-arm64

#RUN git clone --progress --verbose https://github.com/raspberrypi/tools.git --depth=1 pitools

ENV BUILD_FOLDER /build
WORKDIR ${BUILD_FOLDER}

#CMD ["/bin/bash", "-c", "make", "-f", "${BUILD_FOLDER}/Makefile"]
# CMD ["make", "clean"]