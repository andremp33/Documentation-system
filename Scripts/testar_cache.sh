#!/bin/bash

# Parar servidor anterior
pkill dserver 2>/dev/null

# Iniciar servidor com cache de 5 documentos
./bin/dserver mini_dataset 5 &
SERVER_PID=$!
sleep 1

echo "### Indexar todos os ficheiros de mini_dataset/"
for file in mini_dataset/*.txt; do
    filename=$(basename "$file")
    title=$(head -n 1 "$file" | cut -c 1-100)
    ./bin/dclient -a "$title" "Autor" "2025" "$filename"
done

echo ""
echo "### Consultar primeiros 10 documentos para encher a cache"
for id in $(seq 1 10); do
    ./bin/dclient -c "$id"
done

echo ""
echo "### Reconsultar documentos 5 e 6 para ativar LRU"
./bin/dclient -c 5
./bin/dclient -c 6

echo ""
echo "### Parar servidor"
./bin/dclient -f
wait $SERVER_PID
