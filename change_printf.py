import os
for root, _, files in os.walk("Client"):
    for name in files:
        if name.endswith((".cpp", ".hpp")):
            path = os.path.join(root, name)
            with open(path, "r") as f:
                content = f.read()
            new_content = content.replace(" printf(", " DEBUG_PRINT(")
            if content != new_content:
                print(f"Updating {path}")
                with open(path, "w") as f:
                    f.write(new_content)