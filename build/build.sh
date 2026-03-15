# !/bin/bash

function mcp-build {
    (
        cd build
        premake5 gmake || return 1
        make MyCoolVM || return 1
    )
}

function mcp-clean {
    premake5 clean
}