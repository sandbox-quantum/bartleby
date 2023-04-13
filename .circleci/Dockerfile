FROM ubuntu:latest

SHELL ["/bin/bash", "-c"]

RUN apt update
RUN apt install -y clang-15
RUN apt install -y lld-15
RUN apt install -y git
RUN apt install -y rsync
RUN apt install -y graphviz
RUN apt install -y cmake
RUN apt install -y ninja-build
RUN apt install -y curl

ENV CC=clang-15
ENV CXX=clang++-15
ENV LD=lld-15
ENV LLVM_CONFIG=/usr/lib/llvm-15/bin/llvm-config

RUN curl -Lo bazelisk-linux-amd64 'https://github.com/bazelbuild/bazelisk/releases/download/v1.16.0/bazelisk-linux-amd64'
RUN sha384sum -c <<< "55d9ef786360f97902c886eff515c5dd2e05985c52d43809645f963b9a7e19ccf87cef3d07d2cfda8c2cf16aed119616  bazelisk-linux-amd64"
RUN mv bazelisk-linux-amd64 /usr/bin/bazel
RUN chmod +x /usr/bin/bazel

RUN curl -Lo /tmp/doxygen.tar.gz 'https://github.com/doxygen/doxygen/releases/download/Release_1_9_6/doxygen-1.9.6.linux.bin.tar.gz'
RUN sha384sum -c <<< "f06f1668637572695d11747a6527797beb4d184f11a5df03ccf87a4a094c2df0e6f38a3a57ea9ec2c3de625e87fc1d8e  /tmp/doxygen.tar.gz"
WORKDIR /tmp
RUN tar xf doxygen.tar.gz doxygen-1.9.6/bin/doxygen
RUN mv doxygen-1.9.6/bin/doxygen /usr/bin/doxygen
RUN chmod +x /usr/bin/doxygen
RUN rm -rf doxygen-1.9.6 doxygen.tar.gz

RUN useradd -ms /bin/bash pawn
USER pawn
WORKDIR /home/pawn
ENV GOOGLE_APPLICATION_CREDENTIALS=/home/pawn/gcloud-service-key.json
