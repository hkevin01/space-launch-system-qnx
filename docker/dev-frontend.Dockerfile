# Frontend dev image
FROM node:20-bullseye

SHELL ["/bin/bash", "-lc"]

RUN npm install -g pnpm yarn vite webpack webpack-cli parcel-bundler esbuild

# Tools for headless browser testing
RUN apt-get update && apt-get install -y --no-install-recommends \
    git curl wget ca-certificates \
    xvfb xauth \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
CMD ["bash"]
