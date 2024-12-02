# Use a base image that can run C++ applications, e.g., Ubuntu
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies for running the executable
RUN apt-get update && apt-get install -y \
    libssl-dev \
    libcpprest-dev \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Set the working directory in the container
WORKDIR /app

# Copy the pre-compiled executable and necessary libraries into the container
COPY ./storage /app/

# If your executable depends on other libraries, you need to copy them as well
# For example:
# COPY ./path/to/dependencies /path/in/container

# Run the executable
CMD ["./storage"]


