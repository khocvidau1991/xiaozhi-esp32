FROM python:3.12-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends ffmpeg \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY may_chu_nhac ./may_chu_nhac

ENV MUSIC_DIR=/music DATA_DIR=/data PORT=8765
VOLUME ["/music", "/data"]
EXPOSE 8765
CMD ["python", "-m", "may_chu_nhac"]
