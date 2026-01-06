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

RUN mkdir -p build logs

COPY --from=builder /app/build/main ./build/
COPY --from=builder /app/build/belt ./build/
COPY --from=builder /app/build/dispatcher ./build/
COPY --from=builder /app/build/express ./build/
COPY --from=builder /app/build/truck ./build/
COPY --from=builder /app/build/terminal ./build/

COPY --from=builder /app/run.sh .

RUN chmod +x run.sh

CMD ["./run.sh"]
