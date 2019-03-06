FROM continuumio/miniconda

MAINTAINER Zero Speech <zerospeech2017@gmail.com>

# Install software dependencies
RUN apt-get -y update
RUN apt-get install -y \
        bzip2 \
        g++ \
        gcc \
        git-core \
        make \
        parallel \
        pkg-config \
        sox \
        vim \
        wget

# Copy code from zerospeech2017 repository
WORKDIR /zerospeech2017
COPY . .
# comment a useless import causing a bug in the tests
RUN sed -i 's|import matplotlib.pyplot as plt|# import matplotlib.pyplot as plt|' \
        ./track1/src/ABXpy/ABXpy/test/test_sampling.py

# Prepare the conda environment
RUN conda create --name zerospeech python=2

# Setup track1
RUN bash -c "source activate zerospeech && \
        ./track1/setup/setup_track1.sh && \
        pytest ./track1/src"

# Setup track2
RUN bash -c "source activate zerospeech && \
        ./track2/setup/setup_track2.sh"

# Activate the zerospeech environment by default
RUN echo "source activate zerospeech" >> /root/.bashrc
