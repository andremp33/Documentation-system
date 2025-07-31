#!/bin/bash

for file in Gdataset/*.txt; do
    title=$(basename "$file" .txt)
    authors="Desconhecido"
    year="2000"
    path=$(basename "$file")

    ./bin/dclient -a "$title" "$authors" "$year" "$path"
done
