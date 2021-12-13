# base image
FROM ubuntu:20.04

# docker image arguments
ARG AARCH64_TOOLCHAIN_LINK=https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-a/10.3-2021.07/binrel/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf.tar.xz
ARG RISCV_TOOLCHAIN_LINK=https://static.dev.sifive.com/dev-tools/freedom-tools/v2020.12/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14.tar.gz
ARG CLANG_VERSION=12

# install dependencies
RUN apt-get update && apt-get install -y \
        git \
        curl \
        xz-utils \
        build-essential \
        gcc-multilib \
        clang-format-$CLANG_VERSION \
        clang-tidy-$CLANG_VERSION \
        cppcheck

# install toolchains
RUN mkdir /opt/aarch64-toolchain && curl $AARCH64_TOOLCHAIN_LINK | tar xJ -C /opt/aarch64-toolchain --strip-components=1 && \
    mkdir /opt/riscv-toolchain && curl $RISCV_TOOLCHAIN_LINK | tar xz -C /opt/riscv-toolchain --strip-components=1

# Add generic non-root user
RUN addgroup bao && adduser -disabled-password --ingroup bao bao

# setup environment
ENV PATH=$PATH:/opt/aarch64-toolchain/bin
ENV PATH=$PATH:/opt/riscv-toolchain/bin

# default startup command
CMD /bin/bash
