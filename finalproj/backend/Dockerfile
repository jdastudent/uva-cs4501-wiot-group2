FROM rust:1-slim-bookworm AS builder

WORKDIR /app/backend

COPY backend/Cargo.toml backend/Cargo.lock ./
COPY backend/src ./src

RUN cargo build --release

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates libsqlite3-0 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app/backend

COPY --from=builder /app/backend/target/release/wiot-backend /usr/local/bin/wiot-backend
COPY backend /app/backend
COPY frontend /app/frontend

ENV APP_ADDRESS=0.0.0.0:3000
EXPOSE 3000

CMD ["wiot-backend"]
