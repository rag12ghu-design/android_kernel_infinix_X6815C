with open('avbtool.py', 'r', encoding='utf-8') as f:
    text = f.read()

target = "with open('/dev/urandom', 'rb') as f:\n          salt = f.read(hash_size)"
replacement = "salt = os.urandom(hash_size)"
if target in text:
    text = text.replace(target, replacement)
    with open('avbtool.py', 'w', encoding='utf-8') as f:
        f.write(text)
    print("Successfully patched avbtool.py")
else:
    print("Target not found")
