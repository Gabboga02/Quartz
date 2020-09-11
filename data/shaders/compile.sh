#!/bin/sh
for i in *.vert *.frag; do
  glslc --target-env=vulkan1.1 "$i" -o "$i.spv"
done
