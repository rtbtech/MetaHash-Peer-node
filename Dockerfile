FROM ubuntu:20.04 AS builder
MAINTAINER oleg@romanenko.ro

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -qq -y update

RUN apt-get -qq -y install --no-install-recommends git cmake libev-dev libgoogle-perftools-dev libfmt-dev make \
                                                   gcc-9 g++-9 libre2-dev libboost-stacktrace-dev autoconf libssl-dev \
                                                   libconfig++-dev autotools-dev automake libltdl-dev libtool libfmt-dev \
                                                   libgmp-dev

RUN   update-alternatives --quiet --remove-all gcc \
    ; update-alternatives --quiet --remove-all g++ \
    ; update-alternatives --quiet --remove-all cc \
    ; update-alternatives --quiet --remove-all cpp \
    ; update-alternatives --quiet --install /usr/bin/gcc gcc /usr/bin/gcc-9 20 \
    ; update-alternatives --quiet --install /usr/bin/cc cc /usr/bin/gcc-9 20 \
    ; update-alternatives --quiet --install /usr/bin/g++ g++ /usr/bin/g++-9 20 \
    ; update-alternatives --quiet --install /usr/bin/cpp cpp /usr/bin/g++-9 20 \
    ; update-alternatives --quiet --config gcc \
    ; update-alternatives --quiet --config cc \
    ; update-alternatives --quiet --config g++ \
    ; update-alternatives --quiet --config cpp

RUN git config --global http.sslverify false

# libsecp256k1
RUN git clone https://github.com/bitcoin-core/secp256k1.git /tmp/libsecp256k1 && cd /tmp/libsecp256k1 \
    && git checkout f39f99be0e6add959f534c03b93044cef066fe09 \
     && ./autogen.sh && ./configure --enable-static --enable-openssl-tests=no --enable-tests=no \
     --enable-exhaustive-tests=no --enable-coverage=no --enable-benchmark=no \
     && make --jobs=`nproc` && make install

RUN ldconfig

COPY . /builder
RUN cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -S /builder -B /builder/app
RUN make --jobs=`nproc` -C /builder/app
CMD ["/bin/bash"]


FROM ubuntu:20.04
MAINTAINER oleg@romanenko.ro

WORKDIR /conf
ENTRYPOINT ["/app/peer_node"]
CMD [""]

COPY --from=builder /usr/lib/x86_64-linux-gnu/libtcmalloc.so.4 /usr/lib/x86_64-linux-gnu/libunwind.so.8 /usr/lib/
RUN ldconfig

COPY --from=builder /builder/app/bin/peer_node /app/
