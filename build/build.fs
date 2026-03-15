function mcp-build
    premake5 gmake
    make MyCoolVM
end

function mcp-clean
    premake5 clean
end
