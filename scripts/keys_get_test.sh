curl -X PUT localhost:9000/store/key1 --data-binary @in/small_shakespeare.txt -o /dev/null
curl -X PUT localhost:9000/store/key2 --data-binary @in/small_shakespeare.txt -o /dev/null
curl -X PUT localhost:9000/store/key3/ --data-binary @in/small_shakespeare.txt -o /dev/null

curl -X GET localhost:9000/keys/