# Multi-purpose backend dev image
FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04

SHELL ["/bin/bash", "-lc"]

ARG DEBIAN_FRONTEND=noninteractive

# Base packages and tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkg-config \
    git curl wget unzip ca-certificates \
    python3 python3-pip python3-venv python3-dev \
    nodejs npm \
    openjdk-17-jdk maven gradle \
    golang-go \
    gdb valgrind lcov \
    netcat-openbsd iputils-ping telnet \
    libx11-6 libx11-dev libxext6 libxext-dev libxrender1 libxrender-dev libxi6 libxtst6 libxfixes3 \
    libxcb1 libxkbcommon-x11-0 libxkbcommon0 libxcb-xinerama0 libwayland-client0 libwayland-cursor0 libwayland-egl1 \
    xvfb xauth x11-apps \
    && rm -rf /var/lib/apt/lists/*

# Node: install pnpm and yarn
RUN npm install -g pnpm yarn

# Python tooling
RUN pip3 install --no-cache-dir \
    pipx \
    pytest pytest-cov pytest-qt \
    black flake8 mypy bandit \
    matplotlib psutil PyQt6

# Create a user for devcontainer use
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd -s /bin/bash --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER $USERNAME
WORKDIR /workspaces/project

ENV QT_QPA_PLATFORM=xcb \
    QT_AUTO_SCREEN_SCALE_FACTOR=1 \
    QT_ENABLE_HIGHDPI_SCALING=1

CMD ["bash"]
