# curl -X GET localhost:9000/store/archive.zip -o out/archive.zip
# curl -X GET localhost:9000/store/2MB.csv -o out/2MB.csv
# curl -X GET localhost:9000/store/73MB.csv -o out/73MB.csv
curl -X GET localhost:9000/store/small_shakespeare.txt -o out/small_shakespeare.txt

# broken GET: non-existent key
# curl -X GET localhost:9000/store/dontexist.txt -o out/dontexist.txt