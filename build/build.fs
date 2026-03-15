function mcp-build
    cd "build"
    premake5 gmake 
    make MyCoolVM
    cd ".."
end

function mcp-clean
    premake5 clean
end