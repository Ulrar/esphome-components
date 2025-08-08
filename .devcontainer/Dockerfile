# Use ESPHome image as base
FROM ghcr.io/esphome/esphome:latest

# Create non-root user and add to necessary groups
ARG USERNAME=esphome
ARG USER_UID=1000
ARG USER_GID=$USER_UID
ENV REMOTE_USERNAME=$USERNAME

# Update and install packages
RUN apt-get update \
    && apt-get install -y \
        usbutils libusb-1.0-0-dev libudev-dev build-essential cmake ninja-build \
        git wget curl tree vim nano htop tmux screen jq socat minicom picocom \
        udev sudo dialog npm nut-client

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME -s /bin/bash \
    && usermod -aG dialout,plugdev $USERNAME \
    && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME
