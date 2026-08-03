FROM ubuntu:22.04
LABEL "about"="CoreFuzzer base img"

ARG DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get upgrade -y && \
    apt-get install -y --no-install-suggests --no-install-recommends \
    gnupg curl ca-certificates systemctl \
    python3-pip python3-setuptools python3-wheel \
    ninja-build build-essential flex bison \
    git cmake iproute2 \
    libsctp-dev libgnutls28-dev libgcrypt-dev libssl-dev \
    libidn11-dev libmongoc-dev libbson-dev libyaml-dev \
    libnghttp2-dev libmicrohttpd-dev libcurl4-gnutls-dev libnghttp2-dev \
    libtins-dev libtalloc-dev libsctp-dev lksctp-tools \
    meson

# Install MongoDB
RUN curl -fsSL https://pgp.mongodb.com/server-6.0.asc | \
    gpg -o /usr/share/keyrings/mongodb-server-6.0.gpg --dearmor && \
    echo "\
deb [arch=amd64,arm64 signed-by=/usr/share/keyrings/mongodb-server-6.0.gpg] \
https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/6.0 multiverse" | \
    tee /etc/apt/sources.list.d/mongodb-org-6.0.list && \
    apt-get update && apt-get install -y mongodb-org

# Install Open5GS
WORKDIR /corefuzzer_deps
COPY open5gs /corefuzzer_deps/open5gs
WORKDIR /corefuzzer_deps/open5gs
RUN git checkout v2.6.6 && rm -rf install && mkdir install
COPY prometheus-client-c /corefuzzer_deps/open5gs/prometheus-client-c
RUN meson build --prefix=/ -Db_coverage=true && \
    ninja -C build
RUN cd build && ninja install && cd .. && cp build/tests/app/5gc /usr/bin/
COPY sample.yaml /corefuzzer_deps/open5gs/build/configs/sample.yaml

# Install UERANSIM
WORKDIR /corefuzzer_deps
COPY UERANSIM_CoreTesting /corefuzzer_deps/ueransim
WORKDIR /corefuzzer_deps/ueransim
RUN make -j`nproc` && cp build/nr-* /usr/bin/

# Install CoreFuzzer dependencies
COPY requirements.txt /corefuzzer/requirements.txt
RUN pip3 install -r /corefuzzer/requirements.txt

# Install perl dependencies for lcov
RUN apt-get install -y libcapture-tiny-perl libdatetime-perl libdevel-cover-perl \
    libdigest-md5-file-perl libfile-spec-perl libjson-xs-perl \
    libmodule-load-conditional-perl libscalar-list-utils-perl libtime-hires-perl 
RUN cpan App::cpanminus && cpanm Memory::Process

# Install lcov
COPY lcov /corefuzzer_deps/lcov
WORKDIR /corefuzzer_deps/lcov
RUN git checkout v2.2 && make install

# Ensure MongoDB directories exist with proper permissions
RUN mkdir -p /var/log/mongodb /var/lib/mongodb && \
    chown -R mongodb:mongodb /var/log/mongodb /var/lib/mongodb

WORKDIR /corefuzzer
ENTRYPOINT mongod --fork --logpath /var/log/mongodb/mongod.log --dbpath /var/lib/mongodb && /bin/bash
