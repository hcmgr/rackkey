# PUT and GET a key as normal
curl -X PUT localhost:9000/store/small_shakespeare.txt --data-binary @in/small_shakespeare.txt
curl -X GET localhost:9000/store/small_shakespeare.txt -o out/small_shakespeare.txt

# DEL
curl -X DELETE localhost:9000/store/small_shakespeare.txt

# errant GET
curl -X GET localhost:9000/store/small_shakespeare.txt -o out/small_shakespeare.txt