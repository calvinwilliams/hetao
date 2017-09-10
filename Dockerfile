# NOTICE :
#   docker pull centos
#   AFTER make -f makefile.Linux && sudo make -f makefile.Linux install , NOT make -f makefile.Linux clean

# build like this : docker build -f Dockerfile -t hetao .

FROM centos

MAINTAINER calvinwilliams@163.com

RUN yum install net-tools -y
RUN yum install telnet -y
RUN yum install lynx -y

COPY src/hetao /usr/local/bin/hetao
COPY src/hetaocheck /usr/local/bin/hetaocheck
COPY src/minihetao /usr/local/bin/minihetao

RUN mkdir -p /etc/hetao
RUN mkdir -p /var/hetao/www
RUN mkdir -p /var/hetao/log

WORKDIR /tmp

RUN echo "then run the following command to start docker container"
RUN echo "docker run -d -v /etc/hetao:/etc/hetao -v /var/hetao/www:/var/hetao/www -v /var/hetao/log:/var/hetao/log -p 80:80 --net=host hetao /usr/local/bin/hetao /etc/hetao/hetao.conf --no-daemon"

# DISCARD :
#   docker run -it -v /media:/media -v /etc/hetao:/etc/hetao -v /var/hetao/www:/var/hetao/www -v /var/hetao/log:/var/hetao/log -p 80:80 --net=host --privileged=true hetao /bin/bash

