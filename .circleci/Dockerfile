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
RUN apt install -y clang-format-15
RUN apt install -y python3-pip

ENV CC=clang-15
ENV CXX=clang++-15
ENV LD=lld-15
ENV LLVM_CONFIG=/usr/lib/llvm-15/bin/llvm-config
ENV CLANG_FORMAT=clang-format-15

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


RUN curl -Lo /tmp/go.tar.gz 'https://go.dev/dl/go1.20.3.linux-amd64.tar.gz'
RUN sha384sum -c <<< "66fccde4a792c96fed6a85c789d0bf0eb954183b9cc70eb10b1c4c485bccfc5cfbebb82966d53d475150ae6fb8670c80  /tmp/go.tar.gz"
WORKDIR /tmp/
RUN tar -C /usr/local -xf /tmp/go.tar.gz
ENV PATH=$PATH:/usr/local/go/bin
RUN go version
ENV GOPATH=/usr/local/go

RUN go install github.com/bazelbuild/buildtools/buildifier@latest
RUN buildifier --version

RUN pip3 install cmake-format

RUN useradd -ms /bin/bash pawn
USER pawn
WORKDIR /home/pawn
ENV GOOGLE_APPLICATION_CREDENTIALS=/home/pawn/gcloud-service-key.json

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
ENV PATH=$PATH:/home/pawn/.cargo/bin
RUN rustup update

RUN go version
RUN buildifier --version
RUN cmake-format --version
