echo @off

for %%i in (*.vert, *.frag) do (
    glslc "%%~i" -o "%%~i.spv"
)

pause