# Dev tools image
FROM mcr.microsoft.com/devcontainers/base:ubuntu-22.04

SHELL ["/bin/bash", "-lc"]

RUN apt-get update && apt-get install -y --no-install-recommends \
    git python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir \
    pre-commit \
    black flake8 isort mypy \
    bandit safety semgrep \
    pytest pytest-cov coverage

WORKDIR /workspace
CMD ["bash"]
