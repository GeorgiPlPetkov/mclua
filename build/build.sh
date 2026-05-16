#!/bin/bash

function mcp-build {
    local config="${1:-debug_x64}"
    premake5 gmake || return 1
    make config="$config" MyCoolVM || return 1
}

function mcp-clean {
    premake5 clean
}

function mcp-rebuild {
    mcp-clean
    mcp-build "${1}"
}
