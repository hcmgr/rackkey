# curl -X PUT localhost:9000/store/images.zip --data-binary @in/images.zip
# curl -X GET localhost:9000/store/images.zip -o out/images.zip
# cd out/
# unzip images.zip
# cd ../
# ./scripts/clean.sh

curl -X PUT localhost:9000/store/small_shakespeare.txt --data-binary @in/small_shakespeare.txt
curl -X GET localhost:9000/store/small_shakespeare.txt -o out/small_shakespeare.txt
diff in/small_shakespeare.txt out/small_shakespeare.txt
# ./scripts/clean.sh

# curl -X PUT localhost:9000/store/archive.zip --data-binary @in/archive.zip
# curl -X GET localhost:9000/store/archive.zip -o out/archive.zip
# cd out/
# unzip archive.zip
# cd ../
# ./scripts/clean.sh

# curl -X PUT localhost:9000/store/10MB.zip --data-binary @in/10MB.zip

# curl -X PUT localhost:9000/store/2MB.csv --data-binary @in/2MB.csv

# curl -X PUT localhost:9000/store/73MB.csv --data-binary @in/73MB.csv
