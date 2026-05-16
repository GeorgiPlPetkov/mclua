function mcp-build -a config
    set -q config[1]; or set config debug_x64
    premake5 gmake
    make config=$config MyCoolVM
end

function mcp-clean
    premake5 clean
end

function mcp-rebuild -a config
    mcp-clean
    mcp-build $config
end
