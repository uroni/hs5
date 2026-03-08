#!/bin/bash

set -e

# pip install openapi-generator-cli

openapi-generator-cli generate -i ../src/api.yml -g python -o ./hs5_api_client --package-name hs5_api