# Dockerfile - Môi trường build wizk-repack cho Ubuntu & Termux dựa trên WizkBuilder
# Tận dụng container image có sẵn từ GitHub Packages: ghcr.io/wizk-modz/builder
ARG BASE_IMAGE=ghcr.io/wizk-modz/builder:cpp-py
FROM ${BASE_IMAGE}

USER root

LABEL maintainer="Wizk <hdshhhyd@gmail.com>"
LABEL org.opencontainers.image.source="https://github.com/Wizk-Modz/WizkBuilder"
LABEL org.opencontainers.image.description="Môi trường build chuẩn Debian package và Termux cho wizk-repack"

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=en_US.UTF-8

# Bổ sung các công cụ đóng gói Debian nếu image base còn thiếu
RUN if command -v apt-get >/dev/null 2>&1; then \
        apt-get update && apt-get install -yq --no-install-recommends \
            debhelper \
            debhelper-compat \
            dpkg-dev \
            fakeroot \
            xz-utils \
            tar \
            gzip \
        && rm -rf /var/lib/apt/lists/*; \
    fi

WORKDIR /workspace
COPY . /workspace

# Biên dịch binary và đóng gói debian package
RUN make clean && make && dpkg-buildpackage -us -uc -b || make

CMD ["/workspace/repack", "--help"]
