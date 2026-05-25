# ========== GO BUILD STAGE ==========
FROM golang:latest AS go-builder

ARG TARGETARCH
ARG TARGETVARIANT
ARG MIHOMO_REF="Meta"
ARG MIHOMO_CACHE_BUST=1

WORKDIR /build

RUN apt-get update && \
    apt-get install -y --no-install-recommends git build-essential && \
    rm -rf /var/lib/apt/lists/*

# Copy bridge module for scheme generation and mihomo_helper build
COPY bridge/go.mod bridge/go.sum bridge/
COPY bridge/mihomo_helper.go bridge/
COPY scripts/ scripts/

RUN echo "MIHOMO_CACHE_BUST=$MIHOMO_CACHE_BUST" && \
    cd bridge && go get github.com/metacubex/mihomo@${MIHOMO_REF}

RUN cd bridge && go get -u all

RUN cd bridge && go mod tidy

RUN echo "==> Generating mihomo_schemes.h and param_compat.h from mihomo source" && \
    go run scripts/generate_schemes.go mihomo_schemes.h && \
    go run scripts/generate_param_compat.go -o param_compat.h

RUN echo "==> Building mihomo_helper for $TARGETARCH (subprocess mode)" && \
    cd bridge && CGO_ENABLED=0 \
    go build -v -ldflags="-s -w" -o mihomo_helper mihomo_helper.go

RUN ls -lh bridge/mihomo_helper mihomo_schemes.h param_compat.h

# ========== C++ BUILD STAGE ==========
FROM debian:latest AS builder
ARG THREADS="4"
ARG SHA=""
ARG VERSION="dev"
ARG BUILD_DATE=""

WORKDIR /

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    git g++ build-essential cmake python3 python3-pip \
    pkg-config curl \
    libcurl4-openssl-dev libpcre2-dev rapidjson-dev \
    libyaml-cpp-dev ca-certificates ninja-build ccache && \
    rm -rf /var/lib/apt/lists/*

# quickjspp
RUN set -xe && \
    git clone --depth=1 https://github.com/ftk/quickjspp.git && \
    cd quickjspp && \
    git submodule update --init && \
    cmake -DCMAKE_BUILD_TYPE=Release . && \
    make quickjs -j ${THREADS} && \
    install -d /usr/lib/quickjs/ && \
    install -m644 quickjs/libquickjs.a /usr/lib/quickjs/ && \
    install -d /usr/include/quickjs/ && \
    install -m644 quickjs/quickjs.h quickjs/quickjs-libc.h /usr/include/quickjs/ && \
    install -m644 quickjspp.hpp /usr/include

# libcron
RUN set -xe && \
    git clone https://github.com/PerMalmberg/libcron --depth=1 && \
    cd libcron && \
    git submodule update --init && \
    cmake -DCMAKE_BUILD_TYPE=Release . && \
    make libcron -j ${THREADS} && \
    install -m644 libcron/out/Release/liblibcron.a /usr/lib/ && \
    install -d /usr/include/libcron/ && \
    install -m644 libcron/include/libcron/* /usr/include/libcron/ && \
    install -d /usr/include/date/ && \
    install -m644 libcron/externals/date/include/date/* /usr/include/date/

# toml11
RUN set -xe && \
    git clone https://github.com/ToruNiina/toml11 --depth=1 && \
    cd toml11 && \
    cmake -DCMAKE_CXX_STANDARD=11 . && \
    make install -j ${THREADS}

# Copy pre-built mihomo_helper and generated headers from go-builder
COPY --from=go-builder /build/bridge/mihomo_helper /usr/bin/mihomo_helper
COPY --from=go-builder /build/mihomo_schemes.h /tmp/mihomo_schemes.h
COPY --from=go-builder /build/param_compat.h /tmp/param_compat.h

# Build subconverter from THIS repository source
WORKDIR /src
COPY . /src

# Replace checked-in headers with freshly generated ones
RUN cp /tmp/mihomo_schemes.h src/parser/mihomo_schemes.h && \
    cp /tmp/param_compat.h src/parser/param_compat.h

# Download latest header-only libraries
RUN set -xe && \
    echo "Downloading cpp-httplib v0.14.3..." && \
    curl -fsSL https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.14.3/httplib.h -o include/httplib.h && \
    echo "Downloading latest nlohmann/json..." && \
    curl -fsSL https://github.com/nlohmann/json/releases/latest/download/json.hpp -o include/nlohmann/json.hpp && \
    echo "Downloading latest inja..." && \
    curl -fsSL https://raw.githubusercontent.com/pantor/inja/master/single_include/inja/inja.hpp -o include/inja.hpp && \
    echo "Downloading latest jpcre2..." && \
    curl -fsSL https://raw.githubusercontent.com/jpcre2/jpcre2/master/src/jpcre2.hpp -o include/jpcre2.hpp && \
    echo "Copying latest quickjspp from compiled source..." && \
    cp /usr/include/quickjspp.hpp include/quickjspp.hpp && \
    echo "All header libraries updated to latest versions"

RUN set -xe && \
    [ -n "${SHA}" ] && sed -i "s/#define BUILD_ID \"\"/#define BUILD_ID \"${SHA}\"/ " src/version.h || true && \
    [ -n "${VERSION}" ] && sed -i "s/#define VERSION \"dev\"/#define VERSION \"${VERSION}\"/" src/version.h || true && \
    [ -n "${BUILD_DATE}" ] && sed -i "s/#define BUILD_DATE \"\"/#define BUILD_DATE \"${BUILD_DATE}\"/" src/version.h || true && \
    export PATH="/usr/lib/ccache:$PATH" && \
    export CCACHE_DIR=/tmp/ccache && \
    export CCACHE_COMPILERCHECK=content && \
    cmake -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=OFF \
    . && \
    ninja -j ${THREADS}

# ========== FINAL STAGE ==========
FROM alpine:latest

ARG VERSION="dev"
ARG SHA=""
ARG BUILD_DATE=""
LABEL \
  org.opencontainers.image.title="subconverter-Aethersailor" \
  org.opencontainers.image.description="subconverter-Aethersailor - enhanced subscription converter" \
  org.opencontainers.image.url="https://github.com/jshir700/subconverter-Aethersailor" \
  org.opencontainers.image.source="https://github.com/jshir700/subconverter-Aethersailor" \
  org.opencontainers.image.licenses="GPL-3.0" \
  org.opencontainers.image.version="${VERSION}" \
  org.opencontainers.image.revision="${SHA}" \
  org.opencontainers.image.created="${BUILD_DATE}" \
  maintainer="jshir700"

RUN apk add --no-cache ca-certificates

COPY --from=builder /src/subconverter /usr/bin/subconverter
COPY --from=builder /usr/bin/mihomo_helper /usr/bin/mihomo_helper
COPY --from=builder /src/base /base/

RUN chmod +x /usr/bin/subconverter /usr/bin/mihomo_helper

ENV TZ=Asia/Shanghai
RUN ln -sf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /base
RUN set -e && \
    printf '%s\n' \
      '#!/bin/sh' \
      'set -e' \
      'CONF="${PREF_PATH:-/base/pref.toml}"' \
      'CONF_DIR="$(dirname "$CONF")"' \
      'mkdir -p "$CONF_DIR"' \
      'if [ ! -f "$CONF" ] && [ -f /base/pref.example.toml ]; then' \
      '  cp /base/pref.example.toml "$CONF"' \
      'fi' \
      'exec /usr/bin/subconverter -f "$CONF"' \
      > /usr/local/bin/start-subconverter && \
    chmod +x /usr/local/bin/start-subconverter
CMD ["/usr/local/bin/start-subconverter"]
EXPOSE 25500/tcp
