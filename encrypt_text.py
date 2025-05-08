key = 0xAF

def encrypt(plain : str):
    encrypted = "{"
    
    for c in plain:
        encrypted += f"{hex(ord(c) ^ key)}, "
    return encrypted[:-2] + "};"

if __name__ == "__main__":
    plain_text = input("Enter a string you want to encrypt: ")
    print(encrypt(plain_text))