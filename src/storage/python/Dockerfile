FROM python:3.9-slim

# Set the working directory inside container
WORKDIR /app

# Install dependencies
COPY requirements.txt /app
RUN pip install --no-cache-dir -r requirements.txt

# Get code
COPY storage_test_server.py /app

## NOTE: docker compose exposes our ports for us

# Run
CMD ["python3", "storage_test_server.py"]
