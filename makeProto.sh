#!/bin/bash

OUTOPTS="--cpp_out=./proto_src"

protoc -I./proto $OUTOPTS ./proto/ping/v1/services.proto
protoc -I./proto $OUTOPTS ./proto/ping/v1/messages.proto
protoc -I./proto $OUTOPTS ./proto/runner/v1/services.proto
protoc -I./proto $OUTOPTS ./proto/runner/v1/messages.proto
protoc -I./proto $OUTOPTS ./proto/connectrpc/messages.proto

