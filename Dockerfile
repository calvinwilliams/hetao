# NOTICE :
#   docker pull centos
#   after make hetao , NOT make clean

# build like this : docker build -f Dockerfile -t hetao .

FROM centos

MAINTAINER calvinwilliams@163.com

RUN yum install net-tools -y
RUN yum install telnet -y
RUN yum install lynx -y

COPY src/hetao /usr/local/bin/hetao
COPY src/hetaocheck /usr/local/bin/hetaocheck
COPY src/minihetao /usr/local/bin/minihetao

WORKDIR /tmp

RUN echo "then run the following command to start docker container"
RUN echo "docker run -d -v /etc/hetao:/etc/hetao -v /var/hetao/www:/var/hetao/www -v /var/hetao/log:/var/hetao/log -p 80:80 hetao /usr/local/bin/hetao /etc/hetao/hetao.conf --no-daemon"

# DISCARD :
#   docker run -it -v /media:/media -v /etc/hetao:/etc/hetao -v /var/hetao/www:/var/hetao/www -v /var/hetao/log:/var/hetao/log -p 80:80 --net=host --privileged=true hetao /bin/bash

