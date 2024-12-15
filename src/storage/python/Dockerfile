FROM python:3.9-slim

# Set the working directory inside container
WORKDIR /app

# Install dependencies
COPY requirements.txt /app
RUN pip install --no-cache-dir -r requirements.txt

# Get code
COPY simple_test.py /app

EXPOSE 8085

## NOTE: docker compose exposes our ports for us

# Run
CMD ["python3", "simple_test.py"]
