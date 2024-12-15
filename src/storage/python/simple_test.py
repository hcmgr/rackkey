from flask import Flask, request

app = Flask(__name__)

@app.route('/<key>', methods=['GET'])
def get_key(key):
    print(f"GET request for key: {key}")
    return f"GET request for key: {key}", 200

@app.route('/<key>', methods=['PUT'])
def put_key(key):
    data = request.data.decode('utf-8')
    print(f"PUT request for key: {key}")
    return f"PUT request for key: {key}", 200

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8085)