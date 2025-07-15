# CacheDB - Redis-Compatible Key-Value Store

A lightweight Redis-compatible cache database server built with C++ and Asio.

## Prerequisites

This project requires the Asio C++ library. Download it from:
https://think-async.com/Asio/

Extract the asio library to the project root directory so that the structure looks like:
```
CacheDB/
├── DB.cpp
├── README.md
├── asio/
│   ├── include/
│   └── ...
```

## Compilation Instructions

### Windows:
```bash
g++ -std=c++17 -D_WIN32_WINNT=0x0601 -I./asio/include server.cpp -o server -lws2_32 -lmswsock
```

### Linux:
```bash
g++ -std=c++17 -I./asio/include server.cpp -o server -lpthread
```

### macOS:
```bash
g++ -std=c++17 -I./asio/include server.cpp -o server
```

## Running the Server

After compilation, run the server:
```bash
./server
```

The server will start listening on port 6379 (default Redis port).

## Supported Commands

- `PING` - Returns PONG
- `SET key value` - Sets a key-value pair
- `GET key` - Retrieves the value for a key

## Testing

You can test the server using `redis-cli` or any Redis-compatible client:
```bash
redis-cli -h 127.0.0.1 -p 6379
```

## Contributions
You are welcome to send Issues and Pull Requests to this repository.

Copyright 2025 - Kalp Shah