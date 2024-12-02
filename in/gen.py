num_blocks = 1000
block_size = 512
total_size = num_blocks * block_size

text_data = "A" * block_size
fn = f'{num_blocks}_block.txt'

with open(fn, 'w') as f:
    for _ in range(num_blocks):
        f.write(text_data)

print(f"Written {num_blocks} blocks to {fn}")
