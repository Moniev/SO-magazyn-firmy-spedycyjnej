FROM alpine:latest AS builder

RUN apk add --no-cache \
  build-base \
  cmake \
  clang \
  clang-extra-tools \
  git \
  linux-headers

WORKDIR /app

COPY . .

RUN make build

FROM alpine:latest

RUN apk add --no-cache \
  libstdc++ \
  bash \
  procps \
  util-linux

WORKDIR /app

COPY --from=builder /app/build/ ./build/
COPY --from=builder /app/run.sh .

RUN chmod +x run.sh

CMD ["./run.sh"]
