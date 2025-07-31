#!/bin/bash
for file in mini_dataset/*.txt; do
    filename=$(basename "$file")
    title=$(head -n 1 "$file" | cut -c 1-100)
    ./bin/dclient -a "$title" "Author" "2023" "$filename"
    echo "Indexed $filename"
done
