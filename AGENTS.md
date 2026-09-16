# HTTP Server MVP — Agent Context

## Goal

This project is a small, single-client HTTP/1.1 server written in C++20 with the POSIX socket API. Its learning goal is to make the boundary between TCP byte streams and HTTP messages explicit; it is not a production server.

## Repository layout

- `main.cpp`: server, header read loop, request-line parser, router, response builder, and full-send helper.
- `client.cpp`: example client that sends one `GET /health` request and reads until EOF.
- `CMakeLists.txt`: defines the `HTTPServer` and `HTTPClient` targets.
- `README.md`: setup instructions, endpoint matrix, raw tests, and documented limitations.

## Build and run

Run these commands in Linux or WSL from the project root:

```bash
cmake -S . -B build
cmake --build build
./build/HTTPServer
./build/HTTPClient
```

The server listens on port `8080`, handles exactly one connection, writes its response, then closes both client and server sockets.

## Request pipeline

```text
recv() → readHeaders() → parseRequest() → routeRequest()
       → buildResponse() → sendAll() → close()
```

- `readHeaders()` accumulates bytes until `\r\n\r\n`; header input is limited to 8 KiB.
- `parseRequest()` accepts exactly `METHOD PATH HTTP/1.1`, and paths must begin with `/`.
- `routeRequest()` supports `GET /health` and `GET /hello`; unsupported methods return 405 and unknown GET paths return 404.
- Invalid request lines return 400.
- `sendAll()` retries partial sends and `EINTR` until the complete response is sent or an error occurs.

## Acceptance checks

Start a fresh server process for each request, then verify:

```bash
curl -i http://127.0.0.1:8080/health      # 200, OK
curl -i http://127.0.0.1:8080/hello       # 200, Merhaba HTTP
curl -i http://127.0.0.1:8080/nope        # 404
curl -i -X POST http://127.0.0.1:8080/hello # 405
```

Also run the malformed-request and split-request `nc` commands in `README.md`; they verify the 400 path and that a request can arrive in multiple TCP reads.

## Constraints

- Preserve the documented single-client, one-request, `Connection: close` behavior unless the task explicitly changes scope.
- Do not treat one `recv()` call as one complete HTTP request.
- Keep response `Content-Length` accurate and ensure each accepted socket is closed on every path.
- No request bodies, keep-alive, pipelining, TLS, static files, or concurrency are currently supported.
- Keep build artifacts and IDE directories out of version control.
