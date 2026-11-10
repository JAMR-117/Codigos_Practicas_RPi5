FROM python:3.11-slim-bookworm
WORKDIR /app
RUN apt-get update && apt-get install -y gcc libc6-dev libgpiod-dev && rm -rf /var/lib/apt/lists/*
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY . .
CMD ["python", "app.py"]
