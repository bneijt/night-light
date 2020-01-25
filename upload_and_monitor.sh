#!/bin/bash
set -e
platformio run --target=upload
platformio device monitor