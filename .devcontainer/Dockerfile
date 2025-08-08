# Use ESPHome image as base
FROM ghcr.io/esphome/esphome:latest

# Create non-root user and add to necessary groups
ARG USERNAME=esphome
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && apt-get update && apt-get install -y sudo dialog \
    && usermod -aG dialout,plugdev $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME
