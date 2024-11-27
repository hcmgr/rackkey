import os
from flask import Flask, jsonify

app = Flask(__name__)

@app.route('/', methods=['GET'])
def hello_world():
    nodeId = os.environ.get("NODE_ID", "unknown") 
    return jsonify(message=f"hello there from node: {nodeId}")

if __name__ == '__main__':
    port = int(os.environ.get("PORT", 8080))
    app.run(host='0.0.0.0', port=port)