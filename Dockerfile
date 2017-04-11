from ubuntu:14.04

MAINTAINER Zero Speech <zerospeech2017@gmail.com>

# build a docker image with the tool used on the zero speech challenge 

RUN apt-get -y update 
RUN apt-get install -y gcc g++ make git-core pkg-config
RUN apt-get install -y wget parallel sox vim bzip2 


# install mini-conda 
WORKDIR /root
RUN wget -c https://repo.continuum.io/miniconda/Miniconda2-latest-Linux-x86_64.sh 
# COPY Miniconda2-latest-Linux-x86_64.sh /root/
RUN bash /root/Miniconda2-latest-Linux-x86_64.sh -b 

# get zerospeech2017 github repository 
RUN echo 'export PATH=/root/miniconda2/bin:$PATH' >> .bashrc
RUN git clone --recursive http://github.com/bootphon/zerospeech2017 /root/zerospeech2017

### install subpackages from zerospeech2017 
WORKDIR /root/zerospeech2017
ENV PATH /root/miniconda2/bin:$PATH
RUN ./track1/setup/setup_track1.sh
RUN ./track2/setup/setup_track2.sh
