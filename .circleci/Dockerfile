FROM debian:bookworm
MAINTAINER Thomas Bailleux <thomas@sandboxaq.com>

SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get install --yes --no-install-recommends \
    bash \
    ca-certificates \
    cmake \
    curl \
    gcc \
    git \
    gnupg \
    graphviz \
    gzip \
    lsb-release \
    make \
    ninja-build \
    openjdk-17-jdk-headless \
    patch \
    python3-pip \
    rsync \
    software-properties-common \
    ssh \
    sudo \
    tar \
    wget \
    zlib1g \
    zlib1g-dev \
    zstd && \
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add - && \
    echo 'deb http://apt.llvm.org/bookworm/ llvm-toolchain-bookworm-17 main' | sudo tee /etc/apt/sources.list && \
    apt-get update && \
    apt-get install --yes --no-install-recommends \
      llvm-17 \
      llvm-17-dev \
      clang-17 \
      clang-format-17 \
      lld-17 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

ENV GOBIN='/usr/local/go/bin'
ENV PATH="${PATH}:${GOBIN}"
RUN wget -qLO- https://go.dev/dl/go1.22.0.linux-amd64.tar.gz | tar -C /usr/local -xz && \
    wget -L https://github.com/bazelbuild/bazelisk/releases/download/v1.19.0/bazelisk-linux-amd64 -O /usr/bin/bazelisk && \
    wget -L https://github.com/bazelbuild/buildtools/releases/download/v6.4.0/buildifier-linux-amd64 -O /usr/bin/buildifier && \
    chmod +x /usr/bin/bazelisk /usr/bin/buildifier && \
    pip3 install cmake-format --break-system-packages

ARG USER="pawn"
RUN useradd -ms /bin/bash "${USER}" && \
    adduser "${USER}" sudo && \
    echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER "${USER}"
WORKDIR "/home/${USER}"
RUN git config --global --add safe.directory '*'
RUN USE_BAZEL_VERSION=6.5.0 bazelisk version

# Cargo
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal
ENV PATH="${PATH}:/home/${USER}/.cargo/bin"
ENV HOME="/home/${USER}"

ENV CC=clang-17
ENV CXX=clang++-17
ENV LD=lld-17
ENV LLVM_CONFIG=/usr/lib/llvm-17/bin/llvm-config
ENV LLVM_DIR=/usr/lib/llvm-17/lib/cmake/
ENV CLANG_FORMAT=clang-format-17
ENV GOOGLE_APPLICATION_CREDENTIALS="$HOME/gcloud-service-key.json"
