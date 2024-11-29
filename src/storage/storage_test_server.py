import os
import sys
import struct
from flask import Flask, jsonify, request

app = Flask(__name__)

def deserialize_blocks(serialized_data):
    """
    Deserialize multiple blocks from the serialized byte data.
    Each block is serialized as:
        1. 4 bytes for the length of key (keySize)
        2. `keySize` bytes for the `key` string
        3. 4 bytes for `blockNum`
        4. 4 bytes for `size`
        5. Remaining bytes for the data (of length `size`).
    """
    index = 0
    blocks = []

    while index < len(serialized_data):
        # 1. Deserialize `key` length (4 bytes)
        if index + 4 > len(serialized_data):
            break
        key_size = struct.unpack('I', serialized_data[index:index + 4])[0]
        index += 4

        # 2. Deserialize `key` (string of `key_size` bytes)
        if index + key_size > len(serialized_data):
            break
        key = serialized_data[index:index + key_size].decode('utf-8')
        index += key_size

        # 3. Deserialize `blockNum` (4 bytes)
        if index + 4 > len(serialized_data):
            break
        block_num = struct.unpack('I', serialized_data[index:index + 4])[0]
        index += 4

        # 4. Deserialize `size` (4 bytes)
        if index + 4 > len(serialized_data):
            break
        size = struct.unpack('I', serialized_data[index:index + 4])[0]
        index += 4

        # 5. Deserialize the actual data (remaining bytes)
        if index + size > len(serialized_data):
            break
        data = serialized_data[index:index + size]

        # Decode data as UTF-8 text (assuming it's text)
        try:
            text_data = data.decode('utf-8')
        except UnicodeDecodeError:
            text_data = "<non-text data>"

        index += size

        # Store the deserialized block in a list
        blocks.append({
            'key': key,
            'block_num': block_num,
            'size': size,
            'data': data
        })

    return blocks

@app.route('/', methods=['PUT'])
def handle_put():
    nodeId = os.environ.get("NODE_ID", "unknown")
    f = sys.stderr
    print(f"\n###########################", file=f)
    print(f"PUT REQ RECEIVED ON NODE: {nodeId}", file=f)
    print(f"###########################\n", file=f)

    # Read the raw byte data from the request body
    serialized_data = request.get_data()

    # Deserialize the blocks
    blocks = deserialize_blocks(serialized_data)

    # Print out the deserialized blocks
    
    for block in blocks:
        print(f"\n###########################", file=f)
        print(f"Key: {block['key']}", file=f)
        print(f"Block Number: {block['block_num']}", file=f)
        print(f"Size: {block['size']}", file=f)
        print(f"Data: {block['data']}", file=f)  # Print data as a hex string for readability
        print(f"###########################\n", file=f)

    return jsonify(message=f"hello there from node: {nodeId}")

if __name__ == '__main__':
    port = int(os.environ.get("PORT", 8080))
    app.run(host='0.0.0.0', port=port)
